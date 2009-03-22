#include "Python/global_llvm_data.h"

#include "Python.h"

#include "llvm/Analysis/Verifier.h"
#include "llvm/ExecutionEngine/ExecutionEngine.h"
#include "llvm/Function.h"
#include "llvm/Module.h"
#include "llvm/ModuleProvider.h"
#include "llvm/Target/TargetData.h"
#include "llvm/Transforms/Scalar.h"

namespace py {

/// Temporarily replaces the ModuleProvider for a particular
/// FunctionPassManager so that it can operate on an arbitrary
/// function.  Unlike ExistingModuleProvider, does not take ownership
/// of the Module.
class TempModuleProvider : public llvm::ModuleProvider {
public:
    TempModuleProvider(llvm::FunctionPassManager &fpm, llvm::Module *m)
        : fpm_(fpm) {
        TheModule = m;
        fpm_.setModuleProvider(this);
    }
    ~TempModuleProvider() {
        fpm_.setModuleProvider(NULL);
        // Stop ~ModuleProvider from deleting the module.
        TheModule = NULL;
    }
    bool materializeFunction(llvm::Function *, std::string * = 0) {
        return false;
    }
    llvm::Module* materializeModule(std::string * = 0) { return TheModule; }

private:
    llvm::FunctionPassManager &fpm_;
};

}  // namespace py

PyGlobalLlvmData *
PyGlobalLlvmData_New()
{
    return new PyGlobalLlvmData;
}

void
PyGlobalLlvmData_Clear(PyGlobalLlvmData *)
{
    // So far, do nothing.
}

void
PyGlobalLlvmData_Free(PyGlobalLlvmData * global_data)
{
    delete global_data;
}

PyGlobalLlvmData::PyGlobalLlvmData()
    : quick_optimizations_(NULL)
{
    std::string error;
    engine_ = llvm::ExecutionEngine::create(
        new llvm::ExistingModuleProvider(new llvm::Module("<dummy>")),
        // Don't force the interpreter (use JIT if possible).
        false,
        &error,
        // JIT slowly, to produce better machine code.  TODO: We'll
        // almost certainly want to make this configurable per
        // function.
        false);
    if (engine_ == NULL) {
        Py_FatalError(error.c_str());
    }

    InitializeQuickOptimizations();
}

void
PyGlobalLlvmData::InitializeQuickOptimizations()
{
    quick_optimizations_.add(new llvm::TargetData(*engine_->getTargetData()));
    quick_optimizations_.add(llvm::createCFGSimplificationPass());
    quick_optimizations_.add(llvm::createPromoteMemoryToRegisterPass());
    quick_optimizations_.add(llvm::createInstructionCombiningPass());
    quick_optimizations_.add(llvm::createJumpThreadingPass());
    quick_optimizations_.add(llvm::createDeadStoreEliminationPass());
    // Make sure the output is still good.
    quick_optimizations_.add(llvm::createVerifierPass());
}

PyGlobalLlvmData::~PyGlobalLlvmData()
{
    delete engine_;
}

void
PyGlobalLlvmData::OptimizeQuickly(llvm::Function &f)
{
    // TODO: Lock this.
    py::TempModuleProvider mp(quick_optimizations_, f.getParent());
    quick_optimizations_.run(f);
}