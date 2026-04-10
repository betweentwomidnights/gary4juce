#pragma once

#include <JuceHeader.h>

namespace plugin_editor_detail
{
    inline const juce::StringArray& getTerryVariationNames()
    {
        static const juce::StringArray names = []()
        {
            juce::StringArray arr;
            arr.add("accordion_folk");
            arr.add("banjo_bluegrass");
            arr.add("piano_classical");
            arr.add("celtic");
            arr.add("strings_quartet");
            arr.add("synth_retro");
            arr.add("synth_modern");
            arr.add("synth_edm");
            arr.add("lofi_chill");
            arr.add("synth_bass");
            arr.add("rock_band");
            arr.add("cinematic_epic");
            arr.add("retro_rpg");
            arr.add("chiptune");
            arr.add("steel_drums");
            arr.add("gamelan_fusion");
            arr.add("music_box");
            arr.add("trap_808");
            arr.add("lo_fi_drums");
            arr.add("boom_bap");
            arr.add("percussion_ensemble");
            arr.add("future_bass");
            arr.add("synthwave_retro");
            arr.add("melodic_techno");
            arr.add("dubstep_wobble");
            arr.add("glitch_hop");
            arr.add("digital_disruption");
            arr.add("circuit_bent");
            arr.add("orchestral_glitch");
            arr.add("vapor_drums");
            arr.add("industrial_textures");
            arr.add("jungle_breaks");
            return arr;
        }();

        return names;
    }
}
