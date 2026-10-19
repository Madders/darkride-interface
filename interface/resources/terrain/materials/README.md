# Default terrain materials

The eight layers every terrain starts with (terrain::LayerStack::defaults, libraries/terrain/src/terrain/Layers.h),
reached as `qrc:///terrain/materials/<name>.jpg` (the colour, 1024²) and `<name>-normal.jpg`, `<name>-roughness.jpg`,
`<name>-occlusion.jpg`, `<name>-height.jpg` (the maps, 512², the size of a slice of the layer map arrays). All are
from CC0 materials on ambientCG (https://ambientcg.com, Creative Commons CC0 1.0 Universal, no attribution
required): the 1K JPG sets' Color, NormalGL (green up, as the engine reads normal maps), Roughness,
AmbientOcclusion and Displacement maps, the maps scaled to 512² with Lanczos (`~/tivoli-sandboxes/editor/tools/ambientcg/`
keeps the sets).

| layer  | ambientCG asset | occlusion map |
|--------|-----------------|---------------|
| grass  | Grass004        | yes           |
| dirt   | Ground037       | yes           |
| rock   | Rock030         | yes           |
| sand   | Ground054       | yes           |
| gravel | Gravel022       | yes           |
| mud    | Ground022       | yes           |
| moss   | Moss001         | yes           |
| snow   | Snow004         | none in the set |

`library.json` lists them for the editor's material chooser: name, the colour texture's URL, the maps' URLs (empty
for none), the tiling in metres and the plain colour drawn until the texture is in.
