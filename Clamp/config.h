#define PLUG_NAME "Clamp Port"
#define PLUG_MFR "clamp-mac"
#define PLUG_VERSION_HEX 0x00010100
#define PLUG_VERSION_STR "1.1.0"
#define PLUG_UNIQUE_ID 'Clmp'
#define PLUG_MFR_ID 'CmMc'
#define PLUG_URL_STR "https://github.com/"
#define PLUG_EMAIL_STR ""
#define PLUG_COPYRIGHT_STR "Unofficial macOS port of Outobugi Clamp"
#define PLUG_CLASS_NAME Clamp

#define BUNDLE_NAME "Clamp"
#define BUNDLE_MFR "clamp-mac"
#define BUNDLE_DOMAIN "com"

#define SHARED_RESOURCES_SUBPATH "Clamp"

#define PLUG_CHANNEL_IO "1-1 2-2"

#define PLUG_LATENCY 0
#define PLUG_TYPE 0
#define PLUG_DOES_MIDI_IN 0
#define PLUG_DOES_MIDI_OUT 0
#define PLUG_DOES_MPE 0
#define PLUG_DOES_STATE_CHUNKS 0
#define PLUG_HAS_UI 1
#define PLUG_WIDTH 400
#define PLUG_HEIGHT 320
#define PLUG_FPS 60
#define PLUG_SHARED_RESOURCES 0
#define PLUG_HOST_RESIZE 0

#define AUV2_ENTRY Clamp_Entry
#define AUV2_ENTRY_STR "Clamp_Entry"
#define AUV2_FACTORY Clamp_Factory
#define AUV2_VIEW_CLASS Clamp_View
#define AUV2_VIEW_CLASS_STR "Clamp_View"

#define AAX_TYPE_IDS 'IEF1', 'IEF2'
#define AAX_TYPE_IDS_AUDIOSUITE 'IEA1', 'IEA2'
#define AAX_PLUG_MFR_STR "clamp-mac"
#define AAX_PLUG_NAME_STR "Clamp Port\nCLMP"
#define AAX_PLUG_CATEGORY_STR "Effect"
#define AAX_DOES_AUDIOSUITE 1

#define VST3_SUBCATEGORY "Fx|Dynamics"

#define CLAP_MANUAL_URL "https://iplug2.github.io/manuals/example_manual.pdf"
#define CLAP_SUPPORT_URL "https://github.com/iPlug2/iPlug2/wiki"
#define CLAP_DESCRIPTION "A lightweight clipper with two algorithms and 6 channel processing modes"
#define CLAP_FEATURES "audio-effect", "distortion"//, "utility"

#define APP_NUM_CHANNELS 2
#define APP_N_VECTOR_WAIT 0
#define APP_MULT 1
#define APP_COPY_AUV3 0
#define APP_SIGNAL_VECTOR_SIZE 64

#define INTER_REGULAR_FN "Inter-Regular.ttf"
#define INTER_SEMIBOLD_FN "Inter-SemiBold.ttf"
#define INTER_BOLD_FN "Inter-Bold.ttf"
