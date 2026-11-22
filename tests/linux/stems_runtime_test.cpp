// SPDX-License-Identifier: AGPL-3.0-only
// Exercise the real installer and CPU inference without a DAW, network or user data.
#include "../../Source/Stems/StemsRuntime.h"
#include <cstdlib>
#include <iostream>
#include <stdexcept>

namespace
{
void require(bool ok, const juce::String& message)
{
    if (!ok)
        throw std::runtime_error(message.toStdString());
}

stems::StemsService::Job waitFor(stems::StemsService& service)
{
    const auto deadline = juce::Time::getMillisecondCounterHiRes() + 120000.0;
    while (service.getJob().running)
    {
        if (juce::Time::getMillisecondCounterHiRes() > deadline)
        {
            std::cerr << "Timed out: " << service.getJob().status << std::endl;
            std::exit(1);
        }
        juce::Thread::sleep(20);
    }
    return service.getJob();
}

struct Scratch
{
    juce::File directory = juce::File::getSpecialLocation(juce::File::tempDirectory)
                               .getNonexistentChildFile("gary-stems-runtime", {}, false);
    juce::File previous = juce::File::getCurrentWorkingDirectory();
    ~Scratch()
    {
        unsetenv("GARY4JUCE_STEMS_PACKAGE_DIR");
        previous.setAsCurrentWorkingDirectory();
        directory.deleteRecursively();
    }
};
}

int main(int argc, char** argv)
{
    try
    {
        require(argc == 2, "usage: gary_stems_runtime_test FIXTURE_DIRECTORY");
        const juce::File fixtures(juce::File::getCurrentWorkingDirectory().getChildFile(argv[1]));
        Scratch scratch;
        const auto local = scratch.directory.getChildFile("packages");
        require(local.createDirectory().wasOk(), "create local package folder");
        const auto host = scratch.directory.getChildFile("host");
        require(host.createDirectory().wasOk() && host.setAsCurrentWorkingDirectory(), "use a foreign host folder");
        const auto& release = stems::pinnedRuntime();
        auto service = std::make_shared<stems::StemsService>(scratch.directory.getChildFile("gary-data"));
        require(service->isRuntimeAvailableForPlatform() && release.packages.size() == 2,
                "Linux has core and Vulkan packages");
        juce::String sums;
        for (const auto& package : release.packages)
        {
            const auto source = fixtures.getChildFile(package.fileName);
            require(source.getSize() == package.sizeBytes, "published size: " + package.fileName);
            require(juce::SHA256(source).toHexString() == package.sha256, "published hash: " + package.fileName);
            require(source.copyFileTo(local.getChildFile(package.fileName)), "copy fixture: " + package.fileName);
            sums += package.sha256 + "  " + package.fileName + "\n";
        }
        require(local.getChildFile("SHA256SUMS").replaceWithText(sums), "write local package checksums");
        setenv("GARY4JUCE_STEMS_PACKAGE_DIR", local.getFullPathName().toRawUTF8(), 1);

        // Keep the size correct so this exercises SHA-256 rejection and staging cleanup.
        const auto core = local.getChildFile(release.packages.front().fileName);
        juce::MemoryBlock bytes;
        require(core.loadFileAsData(bytes) && bytes.getSize() > 0, "read core ZIP");
        static_cast<unsigned char*>(bytes.getData())[bytes.getSize() / 2] ^= 1;
        require(core.replaceWithData(bytes.getData(), bytes.getSize()), "corrupt core ZIP");
        require(service->startRuntimeInstall(), "start corrupt install");
        auto job = waitFor(*service);
        require(!job.succeeded && job.error.contains("SHA-256") && !service->isRuntimeInstalled(),
                "corrupt ZIP is rejected: " + job.error);
        require(!service->getRuntimeDirectory().getSiblingFile(".staging-" + release.tag).exists(),
                "failed install leaves no staged runtime");
        require(fixtures.getChildFile(core.getFileName()).copyFileTo(core), "restore core ZIP");

        require(service->startRuntimeInstall(), "start valid install");
        job = waitFor(*service);
        require(job.succeeded && service->isRuntimeInstalled(), "install verified ZIPs: " + job.error);
        const auto installed = service->getRuntimeDirectory();
        require(installed.getChildFile("libstems.so").existsAsFile()
                    && installed.getChildFile("libstems-ggml-vulkan.so").existsAsFile(),
                "core and Vulkan are extracted together");
        const auto marker = juce::JSON::parse(installed.getChildFile("installed.json"));
        require(marker["tag"].toString() == release.tag && marker["packages"].size() == 2,
                "installed marker records the release and both packages");

        const auto* model = stems::findModel("uvr_denoise_lite");
        require(model != nullptr, "denoise-lite is in the model catalog");
        const auto source = fixtures.getChildFile(model->file.fileName);
        require(source.getSize() == model->file.sizeBytes
                    && juce::SHA256(source).toHexString() == model->file.sha256,
                "verify the CPU test model");
        require(service->getModelsDirectory().createDirectory().wasOk()
                    && source.copyFileTo(service->getModelsDirectory().getChildFile(model->file.fileName)),
                "stage the test model");
        // startTest loads libstems.so by path, finds its CPU backend, and separates
        // three seconds of synthetic stereo audio, checking every output is finite.
        require(service->startTest(model->id, false), "start CPU separation");
        job = waitFor(*service);
        require(job.succeeded && job.status.contains("CPU"), "CPU separation: " + job.error + job.status);
        std::cout << "PASS: pinned Linux packages, corrupt download rejection, installation and "
                  << job.status << std::endl;
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAIL: " << error.what() << std::endl;
        return 1;
    }
}
