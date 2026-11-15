// The one exported symbol of each plugin (ObsSynthCT1, ObsSynth4D, ObsDrums): its code is a static library that
// gives these three functions, so the same code makes the .clap and, through clap-wrapper, the .vst3.
#include <clap/clap.h>

extern "C"
{
    bool obs_entry_init(const char *path);
    void obs_entry_deinit(void);
    const void *obs_entry_factory(const char *id);

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wattributes"
#endif
    CLAP_EXPORT extern const clap_plugin_entry_t clap_entry;
    const clap_plugin_entry_t clap_entry{CLAP_VERSION_INIT, obs_entry_init, obs_entry_deinit, obs_entry_factory};
#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif
}
