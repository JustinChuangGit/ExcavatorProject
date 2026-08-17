from glob import glob
from setuptools import find_packages, setup


PACKAGE_NAME = "excavator_bringup"
EXCAVATOR_MESHES = [
    path
    for path in glob("meshes/excavator/SM_*.obj")
    if not path.endswith("_Internal.obj")
    and not path.endswith("_UV1.obj")
]
EXCAVATOR_MATERIALS = glob("meshes/excavator/*.mtl")


setup(
    name=PACKAGE_NAME,
    version="0.1.0",
    packages=find_packages(),
    data_files=[
        (
            "share/ament_index/resource_index/packages",
            [f"resource/{PACKAGE_NAME}"],
        ),
        (f"share/{PACKAGE_NAME}", ["package.xml"]),
        (f"share/{PACKAGE_NAME}/launch", glob("launch/*.launch.py")),
        (f"share/{PACKAGE_NAME}/config", glob("config/*")),
        (f"share/{PACKAGE_NAME}/urdf", glob("urdf/*")),
        (
            f"share/{PACKAGE_NAME}/meshes/excavator",
            EXCAVATOR_MESHES + EXCAVATOR_MATERIALS,
        ),
    ],
    install_requires=["setuptools"],
    zip_safe=True,
    maintainer="Justin",
    maintainer_email="justin@example.com",
    description="Launch files for the Unreal excavator simulator.",
    license="Apache-2.0",
    tests_require=["pytest"],
    entry_points={
        "console_scripts": [
            "hydraulic_visualizer = "
            "excavator_bringup.hydraulic_visualizer:main",
        ],
    },
)
