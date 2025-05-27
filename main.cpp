#include "PNGLoader.h"
#include "VolumeBuilder.h"
#include "MeshBuilder.h"
#include "FBXExporter.h"

int main(int argc, char** argv) {
    if (argc < 3) {
        printf("Usage: ImageTo3D <input.png> <output.fbx>\n");
        return 1;
    }

    Image2D image = LoadPNG(argv[1]);
    Volume volume = BuildVolumeFromSilhouette(image, 64);
    Mesh3D mesh = GenerateMesh(volume, 5000);
    ExportToFBX(mesh, argv[2]);

    return 0;
}
