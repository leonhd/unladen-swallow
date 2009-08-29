//===- llvm/unittest/Support/ValueHandleTest.cpp - ValueHandle tests --------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "llvm/Support/ValueHandle.h"

#include "llvm/Constants.h"
#include "llvm/Instructions.h"

#include "gtest/gtest.h"

#include <memory>

using namespace llvm;

namespace {

class ValueHandle : public testing::Test {
protected:
  Constant *ConstantV;
  std::auto_ptr<BitCastInst> BitcastV;

  ValueHandle() :
    ConstantV(ConstantInt::get(Type::getInt32Ty(getGlobalContext()), 0)),
    BitcastV(new BitCastInst(ConstantV, Type::getInt32Ty(getGlobalContext()))) {
  }
};

class ConcreteCallbackVH : public CallbackVH {
public:
  ConcreteCallbackVH() : CallbackVH() {}
  ConcreteCallbackVH(Value *V) : CallbackVH(V) {}
};

TEST_F(ValueHandle, WeakVH_BasicOperation) {
  WeakVH WVH(BitcastV.get());
  EXPECT_EQ(BitcastV.get(), WVH);
  WVH = ConstantV;
  EXPECT_EQ(ConstantV, WVH);

  // Make sure I can call a method on the underlying Value.  It
  // doesn't matter which method.
  EXPECT_EQ(Type::getInt32Ty(getGlobalContext()), WVH->getType());
  EXPECT_EQ(Type::getInt32Ty(getGlobalContext()), (*WVH).getType());
}

TEST_F(ValueHandle, WeakVH_Comparisons) {
  WeakVH BitcastWVH(BitcastV.get());
  WeakVH ConstantWVH(ConstantV);

  EXPECT_TRUE(BitcastWVH == BitcastWVH);
  EXPECT_TRUE(BitcastV.get() == BitcastWVH);
  EXPECT_TRUE(BitcastWVH == BitcastV.get());
  EXPECT_FALSE(BitcastWVH == ConstantWVH);

  EXPECT_TRUE(BitcastWVH != ConstantWVH);
  EXPECT_TRUE(BitcastV.get() != ConstantWVH);
  EXPECT_TRUE(BitcastWVH != ConstantV);
  EXPECT_FALSE(BitcastWVH != BitcastWVH);

  // Cast to Value* so comparisons work.
  Value *BV = BitcastV.get();
  Value *CV = ConstantV;
  EXPECT_EQ(BV < CV, BitcastWVH < ConstantWVH);
  EXPECT_EQ(BV <= CV, BitcastWVH <= ConstantWVH);
  EXPECT_EQ(BV > CV, BitcastWVH > ConstantWVH);
  EXPECT_EQ(BV >= CV, BitcastWVH >= ConstantWVH);

  EXPECT_EQ(BV < CV, BitcastV.get() < ConstantWVH);
  EXPECT_EQ(BV <= CV, BitcastV.get() <= ConstantWVH);
  EXPECT_EQ(BV > CV, BitcastV.get() > ConstantWVH);
  EXPECT_EQ(BV >= CV, BitcastV.get() >= ConstantWVH);

  EXPECT_EQ(BV < CV, BitcastWVH < ConstantV);
  EXPECT_EQ(BV <= CV, BitcastWVH <= ConstantV);
  EXPECT_EQ(BV > CV, BitcastWVH > ConstantV);
  EXPECT_EQ(BV >= CV, BitcastWVH >= ConstantV);
}

TEST_F(ValueHandle, WeakVH_FollowsRAUW) {
  WeakVH WVH(BitcastV.get());
  WeakVH WVH_Copy(WVH);
  WeakVH WVH_Recreated(BitcastV.get());
  BitcastV->replaceAllUsesWith(ConstantV);
  EXPECT_EQ(ConstantV, WVH);
  EXPECT_EQ(ConstantV, WVH_Copy);
  EXPECT_EQ(ConstantV, WVH_Recreated);
}

TEST_F(ValueHandle, WeakVH_NullOnDeletion) {
  WeakVH WVH(BitcastV.get());
  WeakVH WVH_Copy(WVH);
  WeakVH WVH_Recreated(BitcastV.get());
  BitcastV.reset();
  Value *null_value = NULL;
  EXPECT_EQ(null_value, WVH);
  EXPECT_EQ(null_value, WVH_Copy);
  EXPECT_EQ(null_value, WVH_Recreated);
}


TEST_F(ValueHandle, AssertingVH_BasicOperation) {
  AssertingVH<CastInst> AVH(BitcastV.get());
  CastInst *implicit_to_exact_type = AVH;
  implicit_to_exact_type = implicit_to_exact_type;  // Avoid warning.

  AssertingVH<Value> GenericAVH(BitcastV.get());
  EXPECT_EQ(BitcastV.get(), GenericAVH);
  GenericAVH = ConstantV;
  EXPECT_EQ(ConstantV, GenericAVH);

  // Make sure I can call a method on the underlying CastInst.  It
  // doesn't matter which method.
  EXPECT_FALSE(AVH->mayWriteToMemory());
  EXPECT_FALSE((*AVH).mayWriteToMemory());
}

TEST_F(ValueHandle, AssertingVH_Const) {
  const CastInst *ConstBitcast = BitcastV.get();
  AssertingVH<const CastInst> AVH(ConstBitcast);
  const CastInst *implicit_to_exact_type = AVH;
  implicit_to_exact_type = implicit_to_exact_type;  // Avoid warning.
}

TEST_F(ValueHandle, AssertingVH_Comparisons) {
  AssertingVH<Value> BitcastAVH(BitcastV.get());
  AssertingVH<Value> ConstantAVH(ConstantV);

  EXPECT_TRUE(BitcastAVH == BitcastAVH);
  EXPECT_TRUE(BitcastV.get() == BitcastAVH);
  EXPECT_TRUE(BitcastAVH == BitcastV.get());
  EXPECT_FALSE(BitcastAVH == ConstantAVH);

  EXPECT_TRUE(BitcastAVH != ConstantAVH);
  EXPECT_TRUE(BitcastV.get() != ConstantAVH);
  EXPECT_TRUE(BitcastAVH != ConstantV);
  EXPECT_FALSE(BitcastAVH != BitcastAVH);

  // Cast to Value* so comparisons work.
  Value *BV = BitcastV.get();
  Value *CV = ConstantV;
  EXPECT_EQ(BV < CV, BitcastAVH < ConstantAVH);
  EXPECT_EQ(BV <= CV, BitcastAVH <= ConstantAVH);
  EXPECT_EQ(BV > CV, BitcastAVH > ConstantAVH);
  EXPECT_EQ(BV >= CV, BitcastAVH >= ConstantAVH);

  EXPECT_EQ(BV < CV, BitcastV.get() < ConstantAVH);
  EXPECT_EQ(BV <= CV, BitcastV.get() <= ConstantAVH);
  EXPECT_EQ(BV > CV, BitcastV.get() > ConstantAVH);
  EXPECT_EQ(BV >= CV, BitcastV.get() >= ConstantAVH);

  EXPECT_EQ(BV < CV, BitcastAVH < ConstantV);
  EXPECT_EQ(BV <= CV, BitcastAVH <= ConstantV);
  EXPECT_EQ(BV > CV, BitcastAVH > ConstantV);
  EXPECT_EQ(BV >= CV, BitcastAVH >= ConstantV);
}

TEST_F(ValueHandle, AssertingVH_DoesNotFollowRAUW) {
  AssertingVH<Value> AVH(BitcastV.get());
  BitcastV->replaceAllUsesWith(ConstantV);
  EXPECT_EQ(BitcastV.get(), AVH);
}

#ifdef NDEBUG

TEST_F(ValueHandle, AssertingVH_ReducesToPointer) {
  EXPECT_EQ(sizeof(CastInst *), sizeof(AssertingVH<CastInst>));
}

#else  // !NDEBUG

#ifdef GTEST_HAS_DEATH_TEST

TEST_F(ValueHandle, AssertingVH_Asserts) {
  AssertingVH<Value> AVH(BitcastV.get());
  EXPECT_DEATH({BitcastV.reset();},
               "An asserting value handle still pointed to this value!");
  AssertingVH<Value> Copy(AVH);
  AVH = NULL;
  EXPECT_DEATH({BitcastV.reset();},
               "An asserting value handle still pointed to this value!");
  Copy = NULL;
  BitcastV.reset();
}

#endif  // GTEST_HAS_DEATH_TEST

#endif  // NDEBUG

TEST_F(ValueHandle, CallbackVH_BasicOperation) {
  ConcreteCallbackVH CVH(BitcastV.get());
  EXPECT_EQ(BitcastV.get(), CVH);
  CVH = ConstantV;
  EXPECT_EQ(ConstantV, CVH);

  // Make sure I can call a method on the underlying Value.  It
  // doesn't matter which method.
  EXPECT_EQ(Type::getInt32Ty(getGlobalContext()), CVH->getType());
  EXPECT_EQ(Type::getInt32Ty(getGlobalContext()), (*CVH).getType());
}

TEST_F(ValueHandle, CallbackVH_Comparisons) {
  ConcreteCallbackVH BitcastCVH(BitcastV.get());
  ConcreteCallbackVH ConstantCVH(ConstantV);

  EXPECT_TRUE(BitcastCVH == BitcastCVH);
  EXPECT_TRUE(BitcastV.get() == BitcastCVH);
  EXPECT_TRUE(BitcastCVH == BitcastV.get());
  EXPECT_FALSE(BitcastCVH == ConstantCVH);

  EXPECT_TRUE(BitcastCVH != ConstantCVH);
  EXPECT_TRUE(BitcastV.get() != ConstantCVH);
  EXPECT_TRUE(BitcastCVH != ConstantV);
  EXPECT_FALSE(BitcastCVH != BitcastCVH);

  // Cast to Value* so comparisons work.
  Value *BV = BitcastV.get();
  Value *CV = ConstantV;
  EXPECT_EQ(BV < CV, BitcastCVH < ConstantCVH);
  EXPECT_EQ(BV <= CV, BitcastCVH <= ConstantCVH);
  EXPECT_EQ(BV > CV, BitcastCVH > ConstantCVH);
  EXPECT_EQ(BV >= CV, BitcastCVH >= ConstantCVH);

  EXPECT_EQ(BV < CV, BitcastV.get() < ConstantCVH);
  EXPECT_EQ(BV <= CV, BitcastV.get() <= ConstantCVH);
  EXPECT_EQ(BV > CV, BitcastV.get() > ConstantCVH);
  EXPECT_EQ(BV >= CV, BitcastV.get() >= ConstantCVH);

  EXPECT_EQ(BV < CV, BitcastCVH < ConstantV);
  EXPECT_EQ(BV <= CV, BitcastCVH <= ConstantV);
  EXPECT_EQ(BV > CV, BitcastCVH > ConstantV);
  EXPECT_EQ(BV >= CV, BitcastCVH >= ConstantV);
}

TEST_F(ValueHandle, CallbackVH_CallbackOnDeletion) {
  class RecordingVH : public CallbackVH {
  public:
    int DeletedCalls;
    int AURWCalls;

    RecordingVH() : DeletedCalls(0), AURWCalls(0) {}
    RecordingVH(Value *V) : CallbackVH(V), DeletedCalls(0), AURWCalls(0) {}

  private:
    virtual void deleted() { DeletedCalls++; CallbackVH::deleted(); }
    virtual void allUsesReplacedWith(Value *) { AURWCalls++; }
  };

  RecordingVH RVH;
  RVH = BitcastV.get();
  EXPECT_EQ(0, RVH.DeletedCalls);
  EXPECT_EQ(0, RVH.AURWCalls);
  BitcastV.reset();
  EXPECT_EQ(1, RVH.DeletedCalls);
  EXPECT_EQ(0, RVH.AURWCalls);
}

TEST_F(ValueHandle, CallbackVH_CallbackOnRAUW) {
  class RecordingVH : public CallbackVH {
  public:
    int DeletedCalls;
    Value *AURWArgument;

    RecordingVH() : DeletedCalls(0), AURWArgument(NULL) {}
    RecordingVH(Value *V)
      : CallbackVH(V), DeletedCalls(0), AURWArgument(NULL) {}

  private:
    virtual void deleted() { DeletedCalls++; CallbackVH::deleted(); }
    virtual void allUsesReplacedWith(Value *new_value) {
      EXPECT_EQ(NULL, AURWArgument);
      AURWArgument = new_value;
    }
  };

  RecordingVH RVH;
  RVH = BitcastV.get();
  EXPECT_EQ(0, RVH.DeletedCalls);
  EXPECT_EQ(NULL, RVH.AURWArgument);
  BitcastV->replaceAllUsesWith(ConstantV);
  EXPECT_EQ(0, RVH.DeletedCalls);
  EXPECT_EQ(ConstantV, RVH.AURWArgument);
}

TEST_F(ValueHandle, CallbackVH_DeletionCanRAUW) {
  class RecoveringVH : public CallbackVH {
  public:
    int DeletedCalls;
    Value *AURWArgument;
    LLVMContext *Context;

    RecoveringVH() : DeletedCalls(0), AURWArgument(NULL), 
                     Context(&getGlobalContext()) {}
    RecoveringVH(Value *V)
      : CallbackVH(V), DeletedCalls(0), AURWArgument(NULL), 
        Context(&getGlobalContext()) {}

  private:
    virtual void deleted() {
      getValPtr()->replaceAllUsesWith(Constant::getNullValue(Type::getInt32Ty(getGlobalContext())));
      setValPtr(NULL);
    }
    virtual void allUsesReplacedWith(Value *new_value) {
      ASSERT_TRUE(NULL != getValPtr());
      EXPECT_EQ(1U, getValPtr()->getNumUses());
      EXPECT_EQ(NULL, AURWArgument);
      AURWArgument = new_value;
    }
  };

  // Normally, if a value has uses, deleting it will crash.  However, we can use
  // a CallbackVH to remove the uses before the check for no uses.
  RecoveringVH RVH;
  RVH = BitcastV.get();
  std::auto_ptr<BinaryOperator> BitcastUser(
    BinaryOperator::CreateAdd(RVH, 
                              Constant::getNullValue(Type::getInt32Ty(getGlobalContext()))));
  EXPECT_EQ(BitcastV.get(), BitcastUser->getOperand(0));
  BitcastV.reset();  // Would crash without the ValueHandler.
  EXPECT_EQ(Constant::getNullValue(Type::getInt32Ty(getGlobalContext())), RVH.AURWArgument);
  EXPECT_EQ(Constant::getNullValue(Type::getInt32Ty(getGlobalContext())),
            BitcastUser->getOperand(0));
}

}
