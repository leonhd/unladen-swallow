//===- CIndex.cpp - Clang-C Source Indexing Library -----------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
// 
//===----------------------------------------------------------------------===//
//
// This file implements the Clang-C Source Indexing library.
//
//===----------------------------------------------------------------------===//

#include "clang-c/Index.h"
#include "clang/Index/Program.h"
#include "clang/Index/Indexer.h"
#include "clang/Index/ASTLocation.h"
#include "clang/Index/Utils.h"
#include "clang/AST/DeclVisitor.h"
#include "clang/AST/Decl.h"
#include "clang/Basic/FileManager.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Frontend/ASTUnit.h"
#include <cstdio>
using namespace clang;
using namespace idx;

namespace {

// Translation Unit Visitor.
class TUVisitor : public DeclVisitor<TUVisitor> {
  CXTranslationUnit TUnit;
  CXTranslationUnitIterator Callback;
  CXClientData CData;
  
  void Call(enum CXCursorKind CK, NamedDecl *ND) {
    CXCursor C = { CK, ND };
    Callback(TUnit, C, CData);
  }
public:
  TUVisitor(CXTranslationUnit CTU, 
            CXTranslationUnitIterator cback, CXClientData D) : 
    TUnit(CTU), Callback(cback), CData(D) {}
  
  void VisitTranslationUnitDecl(TranslationUnitDecl *D) {
    VisitDeclContext(dyn_cast<DeclContext>(D));
  }
  void VisitDeclContext(DeclContext *DC) {
    for (DeclContext::decl_iterator
           I = DC->decls_begin(), E = DC->decls_end(); I != E; ++I)
      Visit(*I);
  }
  void VisitTypedefDecl(TypedefDecl *ND) { 
    Call(CXCursor_TypedefDecl, ND); 
  }
  void VisitTagDecl(TagDecl *ND) {
    switch (ND->getTagKind()) {
      case TagDecl::TK_struct:
        Call(CXCursor_StructDecl, ND);
        break;
      case TagDecl::TK_class:
        Call(CXCursor_ClassDecl, ND);
        break;
      case TagDecl::TK_union:
        Call(CXCursor_UnionDecl, ND);
        break;
      case TagDecl::TK_enum:
        Call(CXCursor_EnumDecl, ND);
        break;
    }
  }
  void VisitVarDecl(VarDecl *ND) {
    Call(CXCursor_VarDecl, ND);
  }
  void VisitFunctionDecl(FunctionDecl *ND) {
    Call(ND->isThisDeclarationADefinition() ? CXCursor_FunctionDefn
                                            : CXCursor_FunctionDecl, ND);
  }
  void VisitObjCInterfaceDecl(ObjCInterfaceDecl *ND) {
    Call(CXCursor_ObjCInterfaceDecl, ND);
  }
  void VisitObjCCategoryDecl(ObjCCategoryDecl *ND) {
    Call(CXCursor_ObjCCategoryDecl, ND);
  }
  void VisitObjCProtocolDecl(ObjCProtocolDecl *ND) {
    Call(CXCursor_ObjCProtocolDecl, ND);
  }
  void VisitObjCImplementationDecl(ObjCImplementationDecl *ND) {
    Call(CXCursor_ObjCClassDefn, ND);
  }
  void VisitObjCCategoryImplDecl(ObjCCategoryImplDecl *ND) {
    Call(CXCursor_ObjCCategoryDefn, ND);
  }
};

// Declaration visitor.
class CDeclVisitor : public DeclVisitor<CDeclVisitor> {
  CXDecl CDecl;
  CXDeclIterator Callback;
  CXClientData CData;
  
  void Call(enum CXCursorKind CK, NamedDecl *ND) {
    // Disable the callback when the context is equal to the visiting decl.
    if (CDecl == ND && !clang_isReference(CK))
      return;
    CXCursor C = { CK, ND };
    Callback(CDecl, C, CData);
  }
public:
  CDeclVisitor(CXDecl C, CXDeclIterator cback, CXClientData D) : 
    CDecl(C), Callback(cback), CData(D) {}
    
  void VisitObjCCategoryDecl(ObjCCategoryDecl *ND) {
    // Issue callbacks for the containing class.
    Call(CXCursor_ObjCClassRef, ND);
    // FIXME: Issue callbacks for protocol refs.
    VisitDeclContext(dyn_cast<DeclContext>(ND));
  }
  void VisitObjCInterfaceDecl(ObjCInterfaceDecl *D) {
    // Issue callbacks for super class.
    if (D->getSuperClass())
      Call(CXCursor_ObjCSuperClassRef, D);

    for (ObjCProtocolDecl::protocol_iterator I = D->protocol_begin(), 
         E = D->protocol_end(); I != E; ++I)
      Call(CXCursor_ObjCProtocolRef, *I);
    VisitDeclContext(dyn_cast<DeclContext>(D));
  }
  void VisitObjCProtocolDecl(ObjCProtocolDecl *PID) {
    for (ObjCProtocolDecl::protocol_iterator I = PID->protocol_begin(), 
         E = PID->protocol_end(); I != E; ++I)
      Call(CXCursor_ObjCProtocolRef, *I);
      
    VisitDeclContext(dyn_cast<DeclContext>(PID));
  }
  void VisitTagDecl(TagDecl *D) {
    VisitDeclContext(dyn_cast<DeclContext>(D));
  }
  void VisitObjCImplementationDecl(ObjCImplementationDecl *D) {
    VisitDeclContext(dyn_cast<DeclContext>(D));
  }
  void VisitObjCCategoryImplDecl(ObjCCategoryImplDecl *D) {
    VisitDeclContext(dyn_cast<DeclContext>(D));
  }
  void VisitDeclContext(DeclContext *DC) {
    for (DeclContext::decl_iterator
           I = DC->decls_begin(), E = DC->decls_end(); I != E; ++I)
      Visit(*I);
  }
  void VisitEnumConstantDecl(EnumConstantDecl *ND) {
    Call(CXCursor_EnumConstantDecl, ND);
  }
  void VisitFieldDecl(FieldDecl *ND) {
    Call(CXCursor_FieldDecl, ND);
  }
  void VisitVarDecl(VarDecl *ND) {
    Call(CXCursor_VarDecl, ND);
  }
  void VisitParmVarDecl(ParmVarDecl *ND) {
    Call(CXCursor_ParmDecl, ND);
  }
  void VisitObjCPropertyDecl(ObjCPropertyDecl *ND) {
    Call(CXCursor_ObjCPropertyDecl, ND);
  }
  void VisitObjCIvarDecl(ObjCIvarDecl *ND) {
    Call(CXCursor_ObjCIvarDecl, ND);
  }
  void VisitFunctionDecl(FunctionDecl *ND) {
    if (ND->isThisDeclarationADefinition()) {
      VisitDeclContext(dyn_cast<DeclContext>(ND));
    }
  }
  void VisitObjCMethodDecl(ObjCMethodDecl *ND) {
    if (ND->getBody()) {
      Call(ND->isInstanceMethod() ? CXCursor_ObjCInstanceMethodDefn
                                  : CXCursor_ObjCClassMethodDefn, ND);
      VisitDeclContext(dyn_cast<DeclContext>(ND));
    } else
      Call(ND->isInstanceMethod() ? CXCursor_ObjCInstanceMethodDecl
                                  : CXCursor_ObjCClassMethodDecl, ND);
  }
};

}

extern "C" {

CXIndex clang_createIndex() 
{
  return new Indexer(*new Program(), *new FileManager());
}

// FIXME: need to pass back error info.
CXTranslationUnit clang_createTranslationUnit(
  CXIndex CIdx, const char *ast_filename) 
{
  assert(CIdx && "Passed null CXIndex");
  Indexer *CXXIdx = static_cast<Indexer *>(CIdx);
  std::string astName(ast_filename);
  std::string ErrMsg;
  
  return ASTUnit::LoadFromPCHFile(astName, CXXIdx->getFileManager(), &ErrMsg);
}

const char *clang_getTranslationUnitSpelling(CXTranslationUnit CTUnit)
{
  assert(CTUnit && "Passed null CXTranslationUnit");
  ASTUnit *CXXUnit = static_cast<ASTUnit *>(CTUnit);
  return CXXUnit->getOriginalSourceFileName().c_str();
}

void clang_loadTranslationUnit(CXTranslationUnit CTUnit, 
                               CXTranslationUnitIterator callback,
                               CXClientData CData)
{
  assert(CTUnit && "Passed null CXTranslationUnit");
  ASTUnit *CXXUnit = static_cast<ASTUnit *>(CTUnit);
  ASTContext &Ctx = CXXUnit->getASTContext();
  
  TUVisitor DVisit(CTUnit, callback, CData);
  DVisit.Visit(Ctx.getTranslationUnitDecl());
}

void clang_loadDeclaration(CXDecl Dcl, 
                           CXDeclIterator callback, 
                           CXClientData CData)
{
  assert(Dcl && "Passed null CXDecl");
  
  CDeclVisitor DVisit(Dcl, callback, CData);
  DVisit.Visit(static_cast<Decl *>(Dcl));
}

// Some notes on CXEntity:
//
// - Since the 'ordinary' namespace includes functions, data, typedefs, 
// ObjC interfaces, thecurrent algorithm is a bit naive (resulting in one 
// entity for 2 different types). For example:
//
// module1.m: @interface Foo @end Foo *x;
// module2.m: void Foo(int);
//
// - Since the unique name spans translation units, static data/functions 
// within a CXTranslationUnit are *not* currently represented by entities.
// As a result, there will be no entity for the following:
//
// module.m: static void Foo() { }
//


const char *clang_getDeclarationName(CXEntity)
{
  return "";
}
const char *clang_getURI(CXEntity)
{
  return "";
}

CXEntity clang_getEntity(const char *URI)
{
  return 0;
}

//
// CXDecl Operations.
//
CXEntity clang_getEntityFromDecl(CXDecl)
{
  return 0;
}
const char *clang_getDeclSpelling(CXDecl AnonDecl)
{
  assert(AnonDecl && "Passed null CXDecl");
  NamedDecl *ND = static_cast<NamedDecl *>(AnonDecl);
  
  if (ObjCMethodDecl *OMD = dyn_cast<ObjCMethodDecl>(ND)) {
    return OMD->getSelector().getAsString().c_str();
  }    
  if (ND->getIdentifier())
    return ND->getIdentifier()->getName();
  else 
    return "";
}

const char *clang_getCursorSpelling(CXCursor C)
{
  assert(C.decl && "CXCursor has null decl");
  NamedDecl *ND = static_cast<NamedDecl *>(C.decl);
  
  if (clang_isReference(C.kind)) {
    switch (C.kind) {
      case CXCursor_ObjCSuperClassRef: 
        {
        ObjCInterfaceDecl *OID = dyn_cast<ObjCInterfaceDecl>(ND);
        assert(OID && "clang_getCursorLine(): Missing interface decl");
        return OID->getSuperClass()->getIdentifier()->getName();
        }
      case CXCursor_ObjCClassRef: 
        {
        ObjCCategoryDecl *OID = dyn_cast<ObjCCategoryDecl>(ND);
        assert(OID && "clang_getCursorLine(): Missing category decl");
        return OID->getClassInterface()->getIdentifier()->getName();
        }
      case CXCursor_ObjCProtocolRef: 
        {
        ObjCProtocolDecl *OID = dyn_cast<ObjCProtocolDecl>(ND);
        assert(OID && "clang_getCursorLine(): Missing protocol decl");
        return OID->getIdentifier()->getName();
        }
      default:
        return "<not implemented>";
    }
  }
  return clang_getDeclSpelling(C.decl);
}

const char *clang_getCursorKindSpelling(enum CXCursorKind Kind)
{
  switch (Kind) {
   case CXCursor_FunctionDecl: return "FunctionDecl";
   case CXCursor_TypedefDecl: return "TypedefDecl";
   case CXCursor_EnumDecl: return "EnumDecl";
   case CXCursor_EnumConstantDecl: return "EnumConstantDecl";
   case CXCursor_StructDecl: return "StructDecl";
   case CXCursor_UnionDecl: return "UnionDecl";
   case CXCursor_ClassDecl: return "ClassDecl";
   case CXCursor_FieldDecl: return "FieldDecl";
   case CXCursor_VarDecl: return "VarDecl";
   case CXCursor_ParmDecl: return "ParmDecl";
   case CXCursor_ObjCInterfaceDecl: return "ObjCInterfaceDecl";
   case CXCursor_ObjCCategoryDecl: return "ObjCCategoryDecl";
   case CXCursor_ObjCProtocolDecl: return "ObjCProtocolDecl";
   case CXCursor_ObjCPropertyDecl: return "ObjCPropertyDecl";
   case CXCursor_ObjCIvarDecl: return "ObjCIvarDecl";
   case CXCursor_ObjCInstanceMethodDecl: return "ObjCInstanceMethodDecl";
   case CXCursor_ObjCClassMethodDecl: return "ObjCClassMethodDecl";
   case CXCursor_FunctionDefn: return "FunctionDefn";
   case CXCursor_ObjCInstanceMethodDefn: return "ObjCInstanceMethodDefn";
   case CXCursor_ObjCClassMethodDefn: return "ObjCClassMethodDefn";
   case CXCursor_ObjCClassDefn: return "ObjCClassDefn";
   case CXCursor_ObjCCategoryDefn: return "ObjCCategoryDefn";
   case CXCursor_ObjCSuperClassRef: return "ObjCSuperClassRef";
   case CXCursor_ObjCProtocolRef: return "ObjCProtocolRef";
   case CXCursor_ObjCClassRef: return "ObjCClassRef";
   case CXCursor_InvalidFile: return "InvalidFile";
   case CXCursor_NoDeclFound: return "NoDeclFound";
   case CXCursor_NotImplemented: return "NotImplemented";
   default: return "<not implemented>";
  }
}

static enum CXCursorKind TranslateKind(Decl *D) {
  switch (D->getKind()) {
    case Decl::Function: return CXCursor_FunctionDecl;
    case Decl::Typedef: return CXCursor_TypedefDecl;
    case Decl::Enum: return CXCursor_EnumDecl;
    case Decl::EnumConstant: return CXCursor_EnumConstantDecl;
    case Decl::Record: return CXCursor_StructDecl; // FIXME: union/class
    case Decl::Field: return CXCursor_FieldDecl;
    case Decl::Var: return CXCursor_VarDecl;
    case Decl::ParmVar: return CXCursor_ParmDecl;
    case Decl::ObjCInterface: return CXCursor_ObjCInterfaceDecl;
    case Decl::ObjCMethod: {
      ObjCMethodDecl *MD = dyn_cast<ObjCMethodDecl>(D);
      if (MD->isInstanceMethod())
        return CXCursor_ObjCInstanceMethodDecl;
      return CXCursor_ObjCClassMethodDecl;
    }
    default: break;
  }
  return CXCursor_NotImplemented;
}
//
// CXCursor Operations.
//
CXCursor clang_getCursor(CXTranslationUnit CTUnit, const char *source_name, 
                         unsigned line, unsigned column)
{
  assert(CTUnit && "Passed null CXTranslationUnit");
  ASTUnit *CXXUnit = static_cast<ASTUnit *>(CTUnit);
  
  FileManager &FMgr = CXXUnit->getFileManager();
  const FileEntry *File = FMgr.getFile(source_name, 
                                       source_name+strlen(source_name));  
  if (!File) {
    CXCursor C = { CXCursor_InvalidFile, 0 };
    return C;
  }
  SourceLocation SLoc = 
    CXXUnit->getSourceManager().getLocation(File, line, column);
                                                                
  ASTLocation ALoc = ResolveLocationInAST(CXXUnit->getASTContext(), SLoc);
  
  Decl *Dcl = ALoc.getDecl();
  if (Dcl) {  
    CXCursor C = { TranslateKind(Dcl), Dcl };
    return C;
  }
  CXCursor C = { CXCursor_NoDeclFound, 0 };
  return C;
}

CXCursor clang_getCursorFromDecl(CXDecl AnonDecl)
{
  assert(AnonDecl && "Passed null CXDecl");
  NamedDecl *ND = static_cast<NamedDecl *>(AnonDecl);
  
  CXCursor C = { TranslateKind(ND), ND };
  return C;
}

unsigned clang_isInvalid(enum CXCursorKind K)
{
  return K >= CXCursor_FirstInvalid && K <= CXCursor_LastInvalid;
}

unsigned clang_isDeclaration(enum CXCursorKind K)
{
  return K >= CXCursor_FirstDecl && K <= CXCursor_LastDecl;
}

unsigned clang_isReference(enum CXCursorKind K)
{
  return K >= CXCursor_FirstRef && K <= CXCursor_LastRef;
}

unsigned clang_isDefinition(enum CXCursorKind K)
{
  return K >= CXCursor_FirstDefn && K <= CXCursor_LastDefn;
}

CXCursorKind clang_getCursorKind(CXCursor C)
{
  return C.kind;
}

CXDecl clang_getCursorDecl(CXCursor C) 
{
  return C.decl;
}

static SourceLocation getLocationFromCursor(CXCursor C, 
                                            SourceManager &SourceMgr,
                                            NamedDecl *ND) {
  if (clang_isReference(C.kind)) {
    switch (C.kind) {
      case CXCursor_ObjCSuperClassRef: 
        {
        ObjCInterfaceDecl *OID = dyn_cast<ObjCInterfaceDecl>(ND);
        assert(OID && "clang_getCursorLine(): Missing interface decl");
        return OID->getSuperClassLoc();
        }
      case CXCursor_ObjCProtocolRef: 
        {
        ObjCProtocolDecl *OID = dyn_cast<ObjCProtocolDecl>(ND);
        assert(OID && "clang_getCursorLine(): Missing protocol decl");
        return OID->getLocation();
        }
      default:
        return SourceLocation();
    }
  } else { // We have a declaration or a definition.
    SourceLocation SLoc;
    switch (ND->getKind()) {
      case Decl::ObjCInterface: 
        {
        SLoc = dyn_cast<ObjCInterfaceDecl>(ND)->getClassLoc();
        break;
        }
      case Decl::ObjCProtocol: 
        {
        SLoc = ND->getLocation(); /* FIXME: need to get the name location. */
        break;
        }
      default: 
        {
        SLoc = ND->getLocation();
        break;
        }
    }
    if (SLoc.isInvalid())
      return SourceLocation();
    return SourceMgr.getSpellingLoc(SLoc); // handles macro instantiations.
  }
}

unsigned clang_getCursorLine(CXCursor C)
{
  assert(C.decl && "CXCursor has null decl");
  NamedDecl *ND = static_cast<NamedDecl *>(C.decl);
  SourceManager &SourceMgr = ND->getASTContext().getSourceManager();
  
  SourceLocation SLoc = getLocationFromCursor(C, SourceMgr, ND);
  return SourceMgr.getSpellingLineNumber(SLoc);
}

unsigned clang_getCursorColumn(CXCursor C)
{
  assert(C.decl && "CXCursor has null decl");
  NamedDecl *ND = static_cast<NamedDecl *>(C.decl);
  SourceManager &SourceMgr = ND->getASTContext().getSourceManager();
  
  SourceLocation SLoc = getLocationFromCursor(C, SourceMgr, ND);
  return SourceMgr.getSpellingColumnNumber(SLoc);
}
const char *clang_getCursorSource(CXCursor C) 
{
  assert(C.decl && "CXCursor has null decl");
  NamedDecl *ND = static_cast<NamedDecl *>(C.decl);
  SourceManager &SourceMgr = ND->getASTContext().getSourceManager();
  
  SourceLocation SLoc = getLocationFromCursor(C, SourceMgr, ND);
  return SourceMgr.getBufferName(SLoc);
}

} // end extern "C"
