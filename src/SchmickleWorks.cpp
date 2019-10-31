#include "SchmickleWorks.hpp"

Plugin* pluginInstance;
bool compressJsonNotes = true;
bool debugVerbose = true;  // to do : make this false in shipping
bool debugCapture = false;  // if true, record initial state and subsequent actions
bool groupByGMInstrument = false;
int midiQuantizer = 1;

void init(Plugin* p) {
	pluginInstance = p;
	p->addModel(modelNoteTaker);
	p->addModel(modelSuper8);
}
