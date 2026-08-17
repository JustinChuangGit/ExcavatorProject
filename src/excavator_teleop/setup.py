from setuptools import find_packages, setup


PACKAGE_NAME = "excavator_teleop"


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
    ],
    install_requires=["setuptools"],
    zip_safe=True,
    maintainer="Justin",
    maintainer_email="justin@example.com",
    description="Keyboard and Xbox teleoperation for the Unreal excavator simulator.",
    license="Apache-2.0",
    entry_points={
        "console_scripts": [
            "keyboard = excavator_teleop.keyboard:main",
            "xbox_controller = excavator_teleop.xbox_controller:main",
        ],
    },
)
