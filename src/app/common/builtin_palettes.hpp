#pragma once
// Carbonian's palettes, for the panels where there is no Carbonian (a plugin in a DAW on Windows, say): the keys
// panelkit::loadTheme reads, as carbonian-palette makes them (/usr/share/carbonian/palettes). Generated: do not edit.
#include <array>

namespace panelkit
{
    struct BuiltinPalette
    {
        const char *name, *label;
        const char *kv; // key=value lines
    };

    inline const std::array<BuiltinPalette, 8> kBuiltinPalettes{{
        BuiltinPalette{"amber", "Amber", "background=#140d05\nsurface=#1c1308\nsurface_2=#27190b\nidle_bg=#0d0802\nramp_2=#bd8201\nramp_3=#e39c01\nline=#4d3414\nmuted=#8a6a34\naccent=#ffb000\nforeground=#f5d99a\nlabel=Amber\n"},
        BuiltinPalette{"forest", "Forest", "background=#0c1310\nsurface=#141e19\nsurface_2=#1d2b24\nidle_bg=#09100c\nramp_2=#3c8f68\nramp_3=#47aa7b\nline=#2a3d33\nmuted=#4d6357\naccent=#4fbf8a\nforeground=#c3d0c6\nlabel=Forest\n"},
        BuiltinPalette{"grey", "Grey", "background=#1c1c1c\nsurface=#242424\nsurface_2=#2d2d2d\nidle_bg=#181818\nramp_2=#878787\nramp_3=#9e9e9e\nline=#3c3c3c\nmuted=#6e6e6e\naccent=#b0b0b0\nforeground=#c0c0c0\nlabel=Grey\n"},
        BuiltinPalette{"lilac", "Lilac", "background=#120e18\nsurface=#1a1522\nsurface_2=#2a2236\nidle_bg=#0d0a12\nramp_2=#8458cc\nramp_3=#9a6ae6\nline=#3a2d4e\nmuted=#5a4d6e\naccent=#b07cff\nforeground=#c8c0d8\nlabel=Lilac\n"},
        BuiltinPalette{"ocean", "Ocean", "background=#0b111d\nsurface=#131b2b\nsurface_2=#1c283e\nidle_bg=#080d17\nramp_2=#3c76c0\nramp_3=#478ce4\nline=#283a56\nmuted=#4a5a76\naccent=#4f9dff\nforeground=#c2ccdd\nlabel=Ocean\n"},
        BuiltinPalette{"oliva", "Oliva", "background=#121511\nsurface=#1a1e19\nsurface_2=#242a23\nidle_bg=#0d0f0d\nramp_2=#759368\nramp_3=#8baf7b\nline=#3a4338\nmuted=#5a6358\naccent=#9cc48a\nforeground=#c9d0c8\nlabel=Oliva\n"},
        BuiltinPalette{"solarized", "Solarized", "background=#002b36\nsurface=#073642\nsurface_2=#0d4351\nidle_bg=#00232c\nramp_2=#4e5d9c\nramp_3=#5f69b3\nline=#18505e\nmuted=#586e75\naccent=#6c71c4\nforeground=#93a1a1\nlabel=Solarized\n"},
        BuiltinPalette{"wine", "Wine", "background=#160b10\nsurface=#221219\nsurface_2=#321a25\nidle_bg=#10070b\nramp_2=#9f4160\nramp_3=#bd4d72\nline=#4a2636\nmuted=#6b4655\naccent=#d4567f\nforeground=#dcc9d0\nlabel=Wine\n"},
    }};
} // namespace panelkit
