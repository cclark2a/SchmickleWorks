#if defined(_MSC_VER)
#define M_PI 3.141592f
#endif

#include "rack.hpp"


using namespace rack;


extern Plugin *plugin;

////////////////////
// module widgets
////////////////////

struct NotesModuleWidget : ModuleWidget {
    NotesModuleWidget();
};
