from conan import ConanFile

class Application(ConanFile):
    settings = "os", "compiler", "arch", "build_type"
    generators = "PkgConfigDeps", "CMakeDeps"

    requires = (
        "rltools/2.1.0@aurora"
    )
