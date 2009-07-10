//===-- PIC16DebugInfo.cpp - Implementation for PIC16 Debug Information ======//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source 
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file contains the helper functions for representing debug information.
//
//===----------------------------------------------------------------------===//

#include "PIC16.h"
#include "PIC16DebugInfo.h" 
#include "llvm/GlobalVariable.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/Support/DebugLoc.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

/// PopulateDebugInfo - Populate the TypeNo, Aux[] and TagName from Ty.
///
void PIC16DbgInfo::PopulateDebugInfo (DIType Ty, unsigned short &TypeNo,
                                      bool &HasAux, int Aux[], 
                                      std::string &TagName) {
  if (Ty.isBasicType(Ty.getTag())) 
    PopulateBasicTypeInfo (Ty, TypeNo);
  else if (Ty.isDerivedType(Ty.getTag())) 
    PopulateDerivedTypeInfo (Ty, TypeNo, HasAux, Aux, TagName);
  else if (Ty.isCompositeType(Ty.getTag())) 
    PopulateCompositeTypeInfo (Ty, TypeNo, HasAux, Aux, TagName);
  else {
    TypeNo = PIC16Dbg::T_NULL;
    HasAux = false;
  }
  return;
}

/// PopulateBasicTypeInfo- Populate TypeNo for basic type from Ty.
///
void PIC16DbgInfo::PopulateBasicTypeInfo (DIType Ty, unsigned short &TypeNo) {
  std::string Name = "";
  Ty.getName(Name);
  unsigned short BaseTy = GetTypeDebugNumber(Name);
  TypeNo = TypeNo << PIC16Dbg::S_BASIC;
  TypeNo = TypeNo | (0xffff & BaseTy);
}

/// PopulateDerivedTypeInfo - Populate TypeNo, Aux[], TagName for derived type 
/// from Ty. Derived types are mostly pointers.
///
void PIC16DbgInfo::PopulateDerivedTypeInfo (DIType Ty, unsigned short &TypeNo,
                                            bool &HasAux, int Aux[],
                                            std::string &TagName) {

  switch(Ty.getTag())
  {
    case dwarf::DW_TAG_pointer_type:
      TypeNo = TypeNo << PIC16Dbg::S_DERIVED;
      TypeNo = TypeNo | PIC16Dbg::DT_PTR;
      break;
    default:
      TypeNo = TypeNo << PIC16Dbg::S_DERIVED;
  }
  
  // We also need to encode the the information about the base type of
  // pointer in TypeNo.
  DIType BaseType = DIDerivedType(Ty.getGV()).getTypeDerivedFrom();
  PopulateDebugInfo(BaseType, TypeNo, HasAux, Aux, TagName);
}

/// PopulateArrayTypeInfo - Populate TypeNo, Aux[] for array from Ty.
void PIC16DbgInfo::PopulateArrayTypeInfo (DIType Ty, unsigned short &TypeNo,
                                          bool &HasAux, int Aux[],
                                          std::string &TagName) {

  DICompositeType CTy = DICompositeType(Ty.getGV());
  DIArray Elements = CTy.getTypeArray();
  unsigned short size = 1;
  unsigned short Dimension[4]={0,0,0,0};
  for (unsigned i = 0, N = Elements.getNumElements(); i < N; ++i) {
    DIDescriptor Element = Elements.getElement(i);
    if (Element.getTag() == dwarf::DW_TAG_subrange_type) {
      TypeNo = TypeNo << PIC16Dbg::S_DERIVED;
      TypeNo = TypeNo | PIC16Dbg::DT_ARY;
      DISubrange SubRange = DISubrange(Element.getGV());
      Dimension[i] = SubRange.getHi() - SubRange.getLo() + 1;
      // Each dimension is represented by 2 bytes starting at byte 9.
      Aux[8+i*2+0] = Dimension[i];
      Aux[8+i*2+1] = Dimension[i] >> 8;
      size = size * Dimension[i];
    }
  }
  HasAux = true;
  // In auxillary entry for array, 7th and 8th byte represent array size.
  Aux[6] = size & 0xff;
  Aux[7] = size >> 8;
  DIType BaseType = CTy.getTypeDerivedFrom();
  PopulateDebugInfo(BaseType, TypeNo, HasAux, Aux, TagName);
}

/// PopulateStructOrUnionTypeInfo - Populate TypeNo, Aux[] , TagName for 
/// structure or union.
///
void PIC16DbgInfo::PopulateStructOrUnionTypeInfo (DIType Ty, 
                                                  unsigned short &TypeNo,
                                                  bool &HasAux, int Aux[],
                                                  std::string &TagName) {
  DICompositeType CTy = DICompositeType(Ty.getGV());
  TypeNo = TypeNo << PIC16Dbg::S_BASIC;
  if (Ty.getTag() == dwarf::DW_TAG_structure_type)
    TypeNo = TypeNo | PIC16Dbg::T_STRUCT;
  else
    TypeNo = TypeNo | PIC16Dbg::T_UNION;
  CTy.getName(TagName);
  // UniqueSuffix is .number where number is obtained from
  // llvm.dbg.composite<number>.
  std::string UniqueSuffix = "." + Ty.getGV()->getName().substr(18);
  TagName += UniqueSuffix;
  unsigned short size = CTy.getSizeInBits()/8;
  // 7th and 8th byte represent size.
  HasAux = true;
  Aux[6] = size & 0xff;
  Aux[7] = size >> 8;
}

/// PopulateEnumTypeInfo - Populate TypeNo for enum from Ty.
void PIC16DbgInfo::PopulateEnumTypeInfo (DIType Ty, unsigned short &TypeNo) {
  TypeNo = TypeNo << PIC16Dbg::S_BASIC;
  TypeNo = TypeNo | PIC16Dbg::T_ENUM;
}

/// PopulateCompositeTypeInfo - Populate TypeNo, Aux[] and TagName for 
/// composite types from Ty.
///
void PIC16DbgInfo::PopulateCompositeTypeInfo (DIType Ty, unsigned short &TypeNo,
                                              bool &HasAux, int Aux[],
                                              std::string &TagName) {
  switch (Ty.getTag()) {
    case dwarf::DW_TAG_array_type: {
      PopulateArrayTypeInfo (Ty, TypeNo, HasAux, Aux, TagName);
      break;
    }
    case dwarf:: DW_TAG_union_type:
    case dwarf::DW_TAG_structure_type: {
      PopulateStructOrUnionTypeInfo (Ty, TypeNo, HasAux, Aux, TagName);
      break;
    }
    case dwarf::DW_TAG_enumeration_type: {
      PopulateEnumTypeInfo (Ty, TypeNo);
      break;
    }
    default:
      TypeNo = TypeNo << PIC16Dbg::S_DERIVED;
  }
}

/// GetTypeDebugNumber - Get debug type number for given type.
///
unsigned PIC16DbgInfo::GetTypeDebugNumber(std::string &type)  {
  if (type == "char")
    return PIC16Dbg::T_CHAR;
  else if (type == "short")
    return PIC16Dbg::T_SHORT;
  else if (type == "int")
    return PIC16Dbg::T_INT;
  else if (type == "long")
    return PIC16Dbg::T_LONG;
  else if (type == "unsigned char")
    return PIC16Dbg::T_UCHAR;
  else if (type == "unsigned short")
    return PIC16Dbg::T_USHORT;
  else if (type == "unsigned int")
    return PIC16Dbg::T_UINT;
  else if (type == "unsigned long")
    return PIC16Dbg::T_ULONG;
  else
    return 0;
}
 
/// GetStorageClass - Get storage class for give debug variable.
///
short PIC16DbgInfo::getStorageClass(DIGlobalVariable DIGV) {
  short ClassNo;
  if (PAN::isLocalName(DIGV.getGlobal()->getName())) {
    // Generating C_AUTO here fails due to error in linker. Change it once
    // linker is fixed.
    ClassNo = PIC16Dbg::C_STAT;
  }
  else if (DIGV.isLocalToUnit())
    ClassNo = PIC16Dbg::C_STAT;
  else
    ClassNo = PIC16Dbg::C_EXT;
  return ClassNo;
}

/// BeginModule - Emit necessary debug info to start a Module and do other
/// required initializations.
void PIC16DbgInfo::BeginModule(Module &M) {
  // Emit file directive for module.
  SmallVector<GlobalVariable *, 2> CUs;
  SmallVector<GlobalVariable *, 4> GVs;
  SmallVector<GlobalVariable *, 4> SPs;
  CollectDebugInfoAnchors(M, CUs, GVs, SPs);
  if (!CUs.empty()) {
    // FIXME : What if more then one CUs are present in a module ?
    GlobalVariable *CU = CUs[0];
    EmitDebugDirectives = true;
    SwitchToCU(CU);
  }

  // Emit debug info for decls of composite types.
  EmitCompositeTypeDecls(M);
}

/// Helper to find first valid debug loc for a function.
///
static const DebugLoc GetDebugLocForFunction(const MachineFunction &MF) {
  DebugLoc DL;
  for (MachineFunction::const_iterator I = MF.begin(), E = MF.end();
       I != E; ++I) {
    for (MachineBasicBlock::const_iterator II = I->begin(), E = I->end();
         II != E; ++II) {
      DL = II->getDebugLoc();
      if (!DL.isUnknown())
        return DL;
    }
  }
  return DL;
}

/// BeginFunction - Emit necessary debug info to start a function.
///
void PIC16DbgInfo::BeginFunction(const MachineFunction &MF) {
  if (! EmitDebugDirectives) return;
  
  // Retreive the first valid debug Loc and process it.
  const DebugLoc &DL = GetDebugLocForFunction(MF);
  ChangeDebugLoc(MF, DL, true);

  EmitFunctBeginDI(MF.getFunction());
  
  // Set current line to 0 so that.line directive is genearted after .bf.
  CurLine = 0;
}

/// ChangeDebugLoc - Take necessary steps when DebugLoc changes.
/// CurFile and CurLine may change as a result of this.
///
void PIC16DbgInfo::ChangeDebugLoc(const MachineFunction &MF,  
                                  const DebugLoc &DL, bool IsInBeginFunction) {
  if (! EmitDebugDirectives) return;
  assert (! DL.isUnknown()  && "can't change to invalid debug loc");

  GlobalVariable *CU = MF.getDebugLocTuple(DL).CompileUnit;
  unsigned line = MF.getDebugLocTuple(DL).Line;

  SwitchToCU(CU);
  SwitchToLine(line, IsInBeginFunction);
}

/// SwitchToLine - Emit line directive for a new line.
///
void PIC16DbgInfo::SwitchToLine(unsigned Line, bool IsInBeginFunction) {
  if (CurLine == Line) return;
  if (!IsInBeginFunction)  O << "\n\t.line " << Line << "\n";
  CurLine = Line;
}

/// EndFunction - Emit .ef for end of function.
///
void PIC16DbgInfo::EndFunction(const MachineFunction &MF) {
  if (! EmitDebugDirectives) return;
  EmitFunctEndDI(MF.getFunction(), CurLine);
}

/// EndModule - Emit .eof for end of module.
///
void PIC16DbgInfo::EndModule(Module &M) {
  if (! EmitDebugDirectives) return;
  EmitVarDebugInfo(M);
  if (CurFile != "") O << "\n\t.eof";
}
 
/// EmitCompositeTypeElements - Emit debug information for members of a 
/// composite type.
/// 
void PIC16DbgInfo::EmitCompositeTypeElements (DICompositeType CTy,
                                              std::string UniqueSuffix) { 
  unsigned long Value = 0;
  DIArray Elements = CTy.getTypeArray();
  for (unsigned i = 0, N = Elements.getNumElements(); i < N; i++) {
    DIDescriptor Element = Elements.getElement(i);
    unsigned short TypeNo = 0;
    bool HasAux = false;
    int ElementAux[PIC16Dbg::AuxSize] = { 0 };
    std::string TagName = "";
    std::string ElementName;
    GlobalVariable *GV = Element.getGV();
    DIDerivedType DITy(GV);
    DITy.getName(ElementName);
    unsigned short ElementSize = DITy.getSizeInBits()/8;
    // Get mangleddd name for this structure/union  element.
    std::string MangMemName = ElementName + UniqueSuffix;
    PopulateDebugInfo(DITy, TypeNo, HasAux, ElementAux, TagName);
    short Class = 0;
    if( CTy.getTag() == dwarf::DW_TAG_union_type)
      Class = PIC16Dbg::C_MOU;
    else if  (CTy.getTag() == dwarf::DW_TAG_structure_type)
      Class = PIC16Dbg::C_MOS;
    EmitSymbol(MangMemName, Class, TypeNo, Value);
    if (CTy.getTag() == dwarf::DW_TAG_structure_type)
      Value += ElementSize;
    if (HasAux)
      EmitAuxEntry(MangMemName, ElementAux, PIC16Dbg::AuxSize, TagName);
  }
}

/// EmitCompositeTypeDecls - Emit composite type declarations like structure 
/// and union declarations.
///
void PIC16DbgInfo::EmitCompositeTypeDecls(Module &M) {
  for(iplist<GlobalVariable>::iterator I = M.getGlobalList().begin(),
      E = M.getGlobalList().end(); I != E; I++) {
    // Structures and union declaration's debug info has llvm.dbg.composite
    // in its name.
    // FIXME: Checking and relying on llvm.dbg.composite name is not a good idea.
    if(I->getName().find("llvm.dbg.composite") != std::string::npos) {
      GlobalVariable *GV = cast<GlobalVariable >(I);
      DICompositeType CTy(GV);
      if (CTy.getTag() == dwarf::DW_TAG_union_type ||
          CTy.getTag() == dwarf::DW_TAG_structure_type ) {
        std::string name;
        CTy.getName(name);
        std::string DIVar = I->getName();
        // Get the number after llvm.dbg.composite and make UniqueSuffix from 
        // it.
        std::string UniqueSuffix = "." + DIVar.substr(18);
        std::string MangledCTyName = name + UniqueSuffix;
        unsigned short size = CTy.getSizeInBits()/8;
        int Aux[PIC16Dbg::AuxSize] = {0};
        // 7th and 8th byte represent size of structure/union.
        Aux[6] = size & 0xff;
        Aux[7] = size >> 8;
        // Emit .def for structure/union tag.
        if( CTy.getTag() == dwarf::DW_TAG_union_type)
          EmitSymbol(MangledCTyName, PIC16Dbg::C_UNTAG);
        else if  (CTy.getTag() == dwarf::DW_TAG_structure_type) 
          EmitSymbol(MangledCTyName, PIC16Dbg::C_STRTAG);

        // Emit auxiliary debug information for structure/union tag. 
        EmitAuxEntry(MangledCTyName, Aux, PIC16Dbg::AuxSize);

        // Emit members.
        EmitCompositeTypeElements (CTy, UniqueSuffix);

        // Emit mangled Symbol for end of structure/union.
        std::string EOSSymbol = ".eos" + UniqueSuffix;
        EmitSymbol(EOSSymbol, PIC16Dbg::C_EOS);
        EmitAuxEntry(EOSSymbol, Aux, PIC16Dbg::AuxSize, MangledCTyName);
      }
    }
  }
}

/// EmitFunctBeginDI - Emit .bf for function.
///
void PIC16DbgInfo::EmitFunctBeginDI(const Function *F) {
  std::string FunctName = F->getName();
  if (EmitDebugDirectives) {
    std::string FunctBeginSym = ".bf." + FunctName;
    std::string BlockBeginSym = ".bb." + FunctName;

    int BFAux[PIC16Dbg::AuxSize] = {0};
    BFAux[4] = CurLine;
    BFAux[5] = CurLine >> 8;

    // Emit debug directives for beginning of function.
    EmitSymbol(FunctBeginSym, PIC16Dbg::C_FCN);
    EmitAuxEntry(FunctBeginSym, BFAux, PIC16Dbg::AuxSize);

    EmitSymbol(BlockBeginSym, PIC16Dbg::C_BLOCK);
    EmitAuxEntry(BlockBeginSym, BFAux, PIC16Dbg::AuxSize);
  }
}

/// EmitFunctEndDI - Emit .ef for function end.
///
void PIC16DbgInfo::EmitFunctEndDI(const Function *F, unsigned Line) {
  std::string FunctName = F->getName();
  if (EmitDebugDirectives) {
    std::string FunctEndSym = ".ef." + FunctName;
    std::string BlockEndSym = ".eb." + FunctName;

    // Emit debug directives for end of function.
    EmitSymbol(BlockEndSym, PIC16Dbg::C_BLOCK);
    int EFAux[PIC16Dbg::AuxSize] = {0};
    // 5th and 6th byte stand for line number.
    EFAux[4] = CurLine;
    EFAux[5] = CurLine >> 8;
    EmitAuxEntry(BlockEndSym, EFAux, PIC16Dbg::AuxSize);
    EmitSymbol(FunctEndSym, PIC16Dbg::C_FCN);
    EmitAuxEntry(FunctEndSym, EFAux, PIC16Dbg::AuxSize);
  }
}

/// EmitAuxEntry - Emit Auxiliary debug information.
///
void PIC16DbgInfo::EmitAuxEntry(const std::string VarName, int Aux[], int Num,
                                std::string TagName) {
  O << "\n\t.dim " << VarName << ", 1" ;
  // TagName is emitted in case of structure/union objects.
  if (TagName != "")
    O << ", " << TagName;
  for (int i = 0; i<Num; i++)
    O << "," << Aux[i];
}

/// EmitSymbol - Emit .def for a symbol. Value is offset for the member.
///
void PIC16DbgInfo::EmitSymbol(std::string Name, short Class, unsigned short
                              Type, unsigned long Value) {
  O << "\n\t" << ".def "<< Name << ", type = " << Type << ", class = " 
    << Class;
  if (Value > 0)
    O  << ", value = " << Value;
}

/// EmitVarDebugInfo - Emit debug information for all variables.
///
void PIC16DbgInfo::EmitVarDebugInfo(Module &M) {
  SmallVector<GlobalVariable *, 2> CUs;
  SmallVector<GlobalVariable *, 4> GVs;
  SmallVector<GlobalVariable *, 4> SPs;
  CollectDebugInfoAnchors(M, CUs, GVs, SPs);
  if (GVs.empty())
    return;

  for (SmallVector<GlobalVariable *, 4>::iterator I = GVs.begin(),
         E = GVs.end(); I != E; ++I)  {
    DIGlobalVariable DIGV(*I);
    DIType Ty = DIGV.getType();
    unsigned short TypeNo = 0;
    bool HasAux = false;
    int Aux[PIC16Dbg::AuxSize] = { 0 };
    std::string TagName = "";
    std::string VarName = TAI->getGlobalPrefix()+DIGV.getGlobal()->getName();
    PopulateDebugInfo(Ty, TypeNo, HasAux, Aux, TagName);
    // Emit debug info only if type information is availaible.
    if (TypeNo != PIC16Dbg::T_NULL) {
      O << "\n\t.type " << VarName << ", " << TypeNo;
      short ClassNo = getStorageClass(DIGV);
      O << "\n\t.class " << VarName << ", " << ClassNo;
      if (HasAux) 
        EmitAuxEntry(VarName, Aux, PIC16Dbg::AuxSize, TagName);
    }
  }
  O << "\n";
}

/// SwitchToCU - Switch to a new compilation unit.
///
void PIC16DbgInfo::SwitchToCU(GlobalVariable *CU) {
  // Get the file path from CU.
  DICompileUnit cu(CU);
  std::string DirName, FileName;
  std::string FilePath = cu.getDirectory(DirName) + "/" + 
                         cu.getFilename(FileName);

  // Nothing to do if source file is still same.
  if ( FilePath == CurFile ) return;

  // Else, close the current one and start a new.
  if (CurFile != "") O << "\n\t.eof";
  O << "\n\t.file\t\"" << FilePath << "\"\n" ;
  CurFile = FilePath;
  CurLine = 0;
}

/// EmitEOF - Emit .eof for end of file.
///
void PIC16DbgInfo::EmitEOF() {
  if (CurFile != "")
    O << "\n\t.EOF";
}

