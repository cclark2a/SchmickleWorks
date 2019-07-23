#include "SchmickleWorks.hpp"

Plugin* pluginInstance;

void init(Plugin* p) {
	pluginInstance = p;
	p->addModel(modelNoteTaker);
	p->addModel(modelSuper8);
}
