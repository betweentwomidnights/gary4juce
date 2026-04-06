# gary4juce Update Feeds

`gary4juce` Phase 1 update checks use a static JSON manifest published from this repo's `docs/` folder.

Planned GitHub Pages URLs:

- `https://betweentwomidnights.github.io/gary4juce/updates/gary4juce/stable.json`
- `https://betweentwomidnights.github.io/gary4juce/updates/gary4juce/preview.json`

Production builds use the baked-in stable URL above.

Maintainer preview/testing builds can override the manifest URL at runtime with:

- `GARY4JUCE_UPDATE_MANIFEST_URL`

PowerShell example:

```powershell
$env:GARY4JUCE_UPDATE_MANIFEST_URL="https://betweentwomidnights.github.io/gary4juce/updates/gary4juce/preview.json"
```
