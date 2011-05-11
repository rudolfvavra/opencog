#include "PythonModule.h"

#include "agent_finder_api.h"

// for backward compatibility as from boost 1.46 filesystem 3 is the default
#define BOOST_FILESYSTEM_VERSION 2
#include <boost/filesystem/operations.hpp>

#include <opencog/util/Config.h>
#include <opencog/util/misc.h>

using std::vector;
using std::string;

using namespace opencog;

DECLARE_MODULE(PythonModule);

static const char* DEFAULT_PYTHON_MODULE_PATHS[] = 
{
    "opencog/cython",
    "../opencog/cython/tests",
    DATADIR"/python",
#ifndef WIN32
    "/usr/share/opencog/python",
    "/usr/local/share/opencog/python",
#endif // !WIN32
    NULL
};

PythonModule::PythonModule() : Module()
{
    logger().info("[PythonModule] constructor");
}

PythonModule::~PythonModule()
{
    logger().info("[PythonModule] destructor");
    do_load_py_unregister();
    Py_Finalize();
}

void PythonModule::init()
{
    logger().info("[PythonModule] init");

    // Start up Python (this init method skips registering signal handlers)
    Py_InitializeEx(0);
    PyEval_InitThreads();

    // Add our module directories to the Python interprator's path
    const char** config_paths = DEFAULT_PYTHON_MODULE_PATHS;
    PyRun_SimpleString("paths=[]");

    // Add custom paths for python modules from the config file
    std::vector<std::string> pythonpaths;
    try {
        tokenize(config()["PYTHON_EXTENSION_DIRS"], std::back_inserter(pythonpaths), ", ");
        for (std::vector<std::string>::const_iterator it = pythonpaths.begin();
             it != pythonpaths.end(); ++it) {
            boost::filesystem::path modulePath(*it);
            if (boost::filesystem::exists(modulePath)) {
                PyRun_SimpleString(("paths.append('" + modulePath.string() + "')\n").c_str());
            }
        }
    } catch (InvalidParamException &e) {
        size_t found;
        found = std::string(e.what()).find("parameter not found (PYTHON_EXTENSION_DIRS)");
        OC_ASSERT(found != string::npos, "Unknown error reading PYTHON_EXTENSION_DIRS parameter: %s", e.what());
    }

    // Default paths for python modules
    for (int i = 0; config_paths[i] != NULL; ++i) {
        boost::filesystem::path modulePath(config_paths[i]);
        if (boost::filesystem::exists(modulePath)) {
            PyRun_SimpleString(("paths.append('" + modulePath.string() + "')\n").c_str());
        }
    }
    PyRun_SimpleString("import sys; sys.path = paths + sys.path\n");

    // Initialise the agent_finder module which helps with the Python side of
    // things
    if (import_agent_finder() == -1) {
        PyErr_Print();
        logger().error("[PythonModule] Failed to load helper python module");
    }
    PyRun_SimpleString("print sys.path\n");
    // Register our Python loader request
    do_load_py_register();
}

std::string PythonModule::do_load_py(Request *dummy, std::list<std::string> args)
{
    //AtomSpace *space = CogServer::getAtomSpace();
    if (args.size() == 0) return "Please specify Python module to load.";
    requests_and_agents_t thingsInModule;
    std::string moduleName = args.front();
    std::ostringstream oss;
    if (moduleName.substr(moduleName.size()-3,3) == ".py") {
        oss << "Warning: Python module name should be passed without .py extension" << std::endl;
        moduleName.replace(moduleName.size()-3,3,"");
    }
    thingsInModule = load_module(moduleName);
    if (thingsInModule.agents.size() > 0) {
        bool first = true;
        oss << "Python MindAgents found: ";
        foreach(std::string s, thingsInModule.agents) {
            if (!first) {
                oss << ", ";
                first = false;
            }
            std::cout << s << std::endl;
            oss << s;
        }
        oss << ".";
    } else {
        oss << "No subclasses of opencog.cogserver.MindAgent found.";
        std::cout << "No subclasses of opencog.cogserver.MindAgent found.";
    }
    // TODO save the py_module somewhere
    // ...
    // TODO register the agents
    //CogServer& cogserver = static_cast<CogServer&>(server());
    // How do we initialise a mind agent with a factory that will
    // load with given Python class
    //cogserver.registerAgent(PyMindAgent::info().id, &forgettingFactory);
    // TODO return info on what requests and mindagents were found
    return oss.str();

}

