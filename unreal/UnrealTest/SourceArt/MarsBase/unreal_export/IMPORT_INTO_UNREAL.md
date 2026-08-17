# Mars Base Unreal Import

Source mesh:

`SM_MarsBase_Complete.fbx`

The FBX contains the base, spacecraft, walls, windmills, door, and equipment
as one static mesh. The original ground object is not included.

Verified properties:

- Mesh objects after import: 1
- Dimensions: approximately 9.329 x 11.992 x 6.813 meters
- Vertices: 34,111
- Polygons: 34,657
- Material slots after FBX round-trip: 9
- Pivot: Blender world origin
- Orientation: Z up, negative Y forward
- Export unit: meters with FBX unit conversion enabled

## Unreal destination

Import into:

`/Game/ExcavatorSim/Environment/MarsBase`

Place in:

`/Game/ExcavatorSim/Maps/Mars_ExcavationSite`

## Import settings

- Uniform Import Scale: 1.0
- Combine Meshes: enabled
- Import Mesh: enabled
- Import Materials: enabled
- Import Textures: enabled
- Normal Import Method: Import Normals and Tangents
- Generate Missing Collision: enabled for the first test

After import, the mesh should measure approximately 933 x 1199 x 681
centimeters. A radically different size indicates an import-scale mismatch.

If embedded texture import does not populate every material, import the
contents of the adjacent `Textures` directory into the same Unreal content
folder and connect the corresponding texture to each material's Base Color.
The window material may need Translucent blend mode.

For initial collision testing, set Collision Complexity to
`Use Complex Collision As Simple`. The asset is intended to remain Static.

Place the structure on top of the existing Mars terrain. Do not replace or
delete the project's deformable terrain actor.
