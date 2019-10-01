#include "SchmickleWorks.hpp"

Plugin* pluginInstance;
bool debugVerbose = true;  // to do : make this false in shipping
bool debugCapture = false;  // if true, record initial state and subsequent actions
GroupBy groupBy = GroupBy::GM;

void init(Plugin* p) {
	pluginInstance = p;
	p->addModel(modelNoteTaker);
	p->addModel(modelSuper8);
}
