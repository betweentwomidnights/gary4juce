# gary4juce update feeds

GitHub Pages serves these JSON manifests from the **main** branch's `docs/`.
See the [release flow](../RELEASING.md) for packaging and publication order.

- [Windows stable](https://betweentwomidnights.github.io/gary4juce/updates/gary4juce/stable.json)
- [macOS stable](https://betweentwomidnights.github.io/gary4juce/updates/gary4juce/stable-mac.json)
- [Linux preview](https://betweentwomidnights.github.io/gary4juce/updates/gary4juce/preview-linux.json)
- [Legacy maintainer preview](https://betweentwomidnights.github.io/gary4juce/updates/gary4juce/preview.json)

Linux preview builds select `preview-linux.json` by default and compare the
full preview version, such as `5.0.6-linux-preview.1`. The numeric native plugin
version can still be 5.0.6. The optional build definition `GARY4JUCE_UPDATE_VERSION`
provides the full updater version; the Linux CMake build supplies it from
`GARY4JUCE_RELEASE_VERSION`.

The Linux manifest's `download_url` opens the release page so users can choose
their CPU architecture and plugin/standalone format. Its top-level `sha256` is
null because the release has several assets; `artifacts` lists each filename,
architecture, format, direct download URL and SHA-256. Windows stable and macOS
stable feeds are not promoted by Linux prereleases.

Publish the prerelease and verify its public downloads before updating the
Linux preview feed on main. Use the exact release publication timestamp. The
Linux branch's copy of a manifest does not publish to Pages.

The runtime environment override `GARY4JUCE_UPDATE_MANIFEST_URL` takes
precedence over the platform default. For a Linux maintainer test:

```sh
GARY4JUCE_UPDATE_MANIFEST_URL=https://betweentwomidnights.github.io/gary4juce/updates/gary4juce/preview-linux.json ./gary4juce
```

PowerShell example:

```powershell
$env:GARY4JUCE_UPDATE_MANIFEST_URL="https://betweentwomidnights.github.io/gary4juce/updates/gary4juce/preview.json"
```
