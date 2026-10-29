-- Kopfloser Test-Client: meldet sich ohne Spiel am Server an (automatische Co-op-Tests)
target("FalloutBot")
    set_kind("binary")
    set_group("Tests")
    add_defines("TP_FALLOUT=1")
    add_files("*.cpp")
    add_deps("FalloutEncoding", "CommonLib", "TiltedConnect")
    add_packages(
        "tiltedcore",
        "hopscotch-map",
        "glm",
        "gamenetworkingsockets",
        "libuv",
        "snappy")
    add_defines("STEAMNETWORKINGSOCKETS_STATIC_LINK")
