#include "SchmickleWorks.hpp"

Plugin* pluginInstance;
bool debugVerbose = true;  // to do : make this false in shipping

void init(Plugin* p) {
	pluginInstance = p;
	p->addModel(modelNoteTaker);
	p->addModel(modelSuper8);
}
