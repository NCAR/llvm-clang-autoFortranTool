// Declares clang::SyntaxOnlyAction.
#include "clang/Frontend/FrontendActions.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Tooling.h"
// Declares llvm::cl::extrahelp.
#include "llvm/Support/CommandLine.h"

// recursive converter
#include "clang/AST/ASTConsumer.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendAction.h"
#include "clang/Frontend/CompilerInstance.h"

// lexer and writer
#include "clang/Lex/Lexer.h"
#include "clang/Rewrite/Core/Rewriter.h"

#include <stdio.h>
#include <string>
#include <sstream>

using namespace clang;
using namespace clang::tooling;
using namespace llvm;
using namespace std;

// Apply a custom category to all command-line options so that they are the
// only ones displayed.
static llvm::cl::OptionCategory MyToolCategory("my-tool options");

// CommonOptionsParser declares HelpMessage with a description of the common
// command-line options related to the compilation database and input files.
// It's nice to have this help message in all tools.
static cl::extrahelp CommonHelp(CommonOptionsParser::HelpMessage);

// A help message for this specific tool can be added afterwards.
static cl::extrahelp MoreHelp("\nMore help text...");

//------------format class modules----------------------------------------------------------------------------------------------------
class CToFTypeFormatter {
public:
  QualType c_qualType;

  CToFTypeFormatter(QualType qt);
  string getFortranTypeASString(bool typeWrapper);
  bool isSameType(QualType qt2);
};

class RecordDeclFormatter {
public:
  RecordDecl *recordDecl;
  int mode = ANONYMOUS;
  bool structOrUnion = STRUCT;

  const int ANONYMOUS = 0;
  const int ID_ONLY = 1;
  const int TAG_ONLY = 2;
  const int ID_TAG = 3;
  const int UNION = 0;
  const int STRUCT = 1;

  // Member functions declarations
  RecordDeclFormatter(RecordDecl *r);
  void setMode(int m);
  bool isStruct();
  bool isUnion();
  string getFortranStructASString();
  string getFortranFields();

// private:
  
};



class FunctionDeclFormatter {
public:
  FunctionDecl *funcDecl;

  // Member functions declarations
  FunctionDeclFormatter(FunctionDecl *f, Rewriter &r);
  string getParamsNamesASString();
  string getParamsDeclASString();
  string getFortranFunctDeclASString();
  string getParamsTypesASString();

private:
  QualType returnQType;
  llvm::ArrayRef<ParmVarDecl *> params;
  Rewriter &rewriter;
};

// member function definitions

RecordDeclFormatter::RecordDeclFormatter(RecordDecl *r) {
  recordDecl = r;
};

bool RecordDeclFormatter::isStruct() {
  return structOrUnion == STRUCT;
};

bool RecordDeclFormatter::isUnion() {
  return structOrUnion == UNION;
};

string RecordDeclFormatter::getFortranFields() {
  string fieldsInFortran = "";
  if (!recordDecl->field_empty()) {
    for (auto it = recordDecl->field_begin(); it != recordDecl->field_end(); it++) {
      CToFTypeFormatter tf((*it)->getType());
      string identifier = (*it)->getNameAsString();

      fieldsInFortran += "\t" + tf.getFortranTypeASString(true) + " :: " + identifier + "\n";

      // llvm::outs() << "identifier:" <<(*it)->getNameAsString() 
      // << " type: " << (*it)->getType().getAsString()<< "\n";
    }
  }
  return fieldsInFortran;
}

string RecordDeclFormatter::getFortranStructASString() {
  // struct name: recordDecl->getNameAsString()
  // type,bind(c): (*it)->getNameAsString() if empty chang to (*it)->getType().getAsString()
  // type(c_type) :: var1, var2...
  // end type

  string rd_buffer;

  if (mode == ID_ONLY) {
    string identifier = "struct " + recordDecl->getNameAsString();
    string fieldsInFortran = getFortranFields();

    rd_buffer = "TYPE, BIND(C) :: " + identifier + "\n" + fieldsInFortran + "END TYPE\n";
    
  }

  return rd_buffer;

};

void RecordDeclFormatter::setMode(int m) {
  // int ANONYMOUS = 0;
  // int ID_ONLY = 1;
  // int TAG_ONLY = 2;
  // int ID_TAG = 3;

  if (mode == ID_ONLY and m == TAG_ONLY) {
    mode = ID_TAG;
  } else if (mode == TAG_ONLY and m == ID_ONLY) {
    mode = ID_TAG;
  } else {
    mode = m;
  }
  llvm::outs() << "setmode: " << to_string(m) << " current mode: " << to_string(mode) << "\n";

};

CToFTypeFormatter::CToFTypeFormatter(QualType qt) {
  c_qualType = qt;
};

bool CToFTypeFormatter::isSameType(QualType qt2) {
  // for pointer type, only distinguish between the function pointer and other pointers
  if (c_qualType.getTypePtr()->isPointerType() and qt2.getTypePtr()->isPointerType()) {
    if (c_qualType.getTypePtr()->isFunctionPointerType() and qt2.getTypePtr()->isFunctionPointerType()) {
      return true;
    } else if ((!c_qualType.getTypePtr()->isFunctionPointerType()) and (!qt2.getTypePtr()->isFunctionPointerType())) {
      return true;
    } else {
      return false;
    }
  } else {
    return c_qualType == qt2;
  }
};

string CToFTypeFormatter::getFortranTypeASString(bool typeWrapper) {
  string f_type;
  // support int, c_ptr
  // int -> ineteger(c_int), VALUE
  if (c_qualType.getTypePtr()->isIntegerType()) {
    if (typeWrapper) {
      f_type = "integer(c_int)";
    } else {
      f_type = "c_int";
    }
  } else if (c_qualType.getTypePtr()->isRealType()) {
    if (typeWrapper) {
      f_type = "real(c_double)";
    } else {
      f_type = "c_double";
    }
  } else if (c_qualType.getTypePtr()->isPointerType ()) {
    if (c_qualType.getTypePtr()->isFunctionPointerType()){
      if (typeWrapper) {
        f_type = "type(c_funptr)";
      } else {
        f_type = "c_funptr";
      }
    } else {
      if (typeWrapper) {
        f_type = "type(c_ptr)";
      } else {
        f_type = "c_ptr";
      }
    }
  } else {
    f_type = "not yet implemented";
  }
  return f_type;
};



FunctionDeclFormatter::FunctionDeclFormatter(FunctionDecl *f, Rewriter &r) : rewriter(r) {
  funcDecl = f;
  returnQType = funcDecl->getReturnType();
  params = funcDecl->parameters();
};

string FunctionDeclFormatter::getParamsTypesASString() {
  string paramsType;
  QualType prev_qt;
  std::vector<QualType> qts;
  bool first = true;
  for (auto it = params.begin(); it != params.end(); it++) {
    if (first) {
      prev_qt = (*it)->getOriginalType();
      qts.push_back(prev_qt);
      CToFTypeFormatter tf((*it)->getOriginalType());
      paramsType = tf.getFortranTypeASString(false);
      first = false;
      //llvm::outs() << "first arg " << (*it)->getOriginalType().getAsString() + "\n";


      // add the return type too
      CToFTypeFormatter rtf(returnQType);
      if (!returnQType.getTypePtr()->isVoidType()) {
        if (rtf.isSameType(prev_qt)) {
          //llvm::outs() << "same type as previous" << (*it)->getOriginalType().getAsString() + "\n";
        } else {
          // check if type is in the vector
          bool add = true;
          for (auto v = qts.begin(); v != qts.end(); v++) {
            if (rtf.isSameType(*v)) {
              add = false;
            }
          }
          if (add) {
            paramsType += (", " + rtf.getFortranTypeASString(false));
          }
          //llvm::outs() << "different type as previous" << (*it)->getOriginalType().getAsString() + "\n";
        }
        prev_qt = returnQType;
        qts.push_back(prev_qt);
      }

    } else {
      CToFTypeFormatter tf((*it)->getOriginalType());
      if (tf.isSameType(prev_qt)) {
        //llvm::outs() << "same type as previous" << (*it)->getOriginalType().getAsString() + "\n";
      } else {
        // check if type is in the vector
        bool add = true;
        for (auto v = qts.begin(); v != qts.end(); v++) {
          if (tf.isSameType(*v)) {
            add = false;
          }
        }
        if (add) {
          paramsType += (", " + tf.getFortranTypeASString(false));
        }
        //llvm::outs() << "different type as previous" << (*it)->getOriginalType().getAsString() + "\n";
      }
      prev_qt = (*it)->getOriginalType();
      qts.push_back(prev_qt);
    }
  }
  return paramsType;
}

string FunctionDeclFormatter::getParamsDeclASString() { 
  string paramsDecl;
  int index = 1;
  for (auto it = params.begin(); it != params.end(); it++) {
    // if the param name is empty, rename it to arg_index
    string pname = (*it)->getNameAsString();
    if (pname.empty()) {
      pname = "arg_" + to_string(index);
    }
    
    CToFTypeFormatter tf((*it)->getOriginalType());
    // in some cases parameter doesn't have a name
    paramsDecl += "\t" + tf.getFortranTypeASString(true) + ", value" + " :: " + pname + "\n"; // need to handle the attribute later
    index ++;
  }
  return paramsDecl;
}

string FunctionDeclFormatter::getParamsNamesASString() { 
  string paramsNames;
  int index = 1;
  for (auto it = params.begin(); it != params.end(); it++) {
    if (it == params.begin()) {
    // if the param name is empty, rename it to arg_index
    string pname = (*it)->getNameAsString();
    if (pname.empty()) {
      pname = "arg_" + to_string(index);
    }
      paramsNames += pname;
    } else { // parameters in between
    // if the param name is empty, rename it to arg_index
    string pname = (*it)->getNameAsString();
    if (pname.empty()) {
      pname = "arg_" + to_string(index);
    }
      paramsNames += ", " + pname; 
    }
    index ++;
  }
  return paramsNames;
};

string FunctionDeclFormatter::getFortranFunctDeclASString() {
  string fortanFunctDecl;
  string funcType;
  string imports = "USE iso_c_binding, only: " + getParamsTypesASString();
  // check if the return type is void or not
  if (returnQType.getTypePtr()->isVoidType()) {
    funcType = "SUBROUTINE";
  } else {
    CToFTypeFormatter tf(returnQType);
    funcType = tf.getFortranTypeASString(true) + " FUNCTION";
  }

  fortanFunctDecl = funcType + " " + funcDecl->getNameAsString() + "(" + getParamsNamesASString() + ")" + " bind (C)\n";
  fortanFunctDecl += "\t" + imports + "\n";
  fortanFunctDecl += getParamsDeclASString();
  // preserve the function body as comment
  if (funcDecl->hasBody()) {
    Stmt *stmt = funcDecl->getBody();
    clang::SourceManager &sm = rewriter.getSourceMgr();
    // comment out the entire function {!body...}
    string bodyText = Lexer::getSourceText(CharSourceRange::getTokenRange(stmt->getSourceRange()), sm, LangOptions(), 0);
    string commentedBody = "! comment out function body by default \n";
    std::istringstream in(bodyText);
    for (std::string line; std::getline(in, line);) {
      commentedBody += "! " + line + "\n";
    }
    fortanFunctDecl += commentedBody;

  }
  fortanFunctDecl += "END " + funcType + " " + funcDecl->getNameAsString() + "\n";


  return fortanFunctDecl;
};


//------------helper methods----------------------------------------------------------------------------------------------------

//-----------the program----------------------------------------------------------------------------------------------------

class TraverseNodeVisitor : public RecursiveASTVisitor<TraverseNodeVisitor> {
public:
  TraverseNodeVisitor(Rewriter &R) : TheRewriter(R) {}


  bool TraverseDecl(Decl *d) {
    if (isa<TranslationUnitDecl> (d)) {
      // tranlastion unit decl is the top node of all AST, ignore the inner structure of tud for now
      llvm::outs() << "this is a TranslationUnitDecl\n";
      RecursiveASTVisitor<TraverseNodeVisitor>::TraverseDecl(d);
    } else if (isa<FunctionDecl> (d)) {
          // create formatter
      FunctionDeclFormatter fdf(cast<FunctionDecl> (d), TheRewriter);
      // -------------------------------dump Fortran-------------------------------
      llvm::outs() << fdf.getFortranFunctDeclASString()
      << "\n";

    } else if (isa<TypedefDecl> (d)) {
      //TypedefDecl *tdd = cast<TypedefDecl> (d);
      llvm::outs() << "found TypedefDecl \n";
    } else if (isa<RecordDecl> (d)) {
      // struct
      RecordDeclFormatter rdf(cast<RecordDecl> (d));
      RecordDecl *recordDecl = rdf.recordDecl;

      // check if there is an identifier
      // // NOT WORKING!
      // if (recordDecl->isAnonymousStructOrUnion()) {
      //   llvm::outs() << "is Anonymous\n"; 
      // } else {
      //   llvm::outs() << "is not Anonymous\n"; 
      // }
      if (!(recordDecl->getNameAsString()).empty()) {
        rdf.setMode(rdf.ID_ONLY);
      }

      // assume struct

      // dump the fortran code
      if (rdf.mode == rdf.ID_ONLY or rdf.mode == rdf.ID_TAG) {
        // struct has a identifier 
        llvm::outs() << rdf.getFortranStructASString();
      }





      // llvm::outs() << "found RecordDecl " << recordDecl->getNameAsString() + "\n";

      //RecursiveASTVisitor<TraverseNodeVisitor>::TraverseDecl(d);
    } else if (isa<VarDecl> (d)) {
      VarDecl *varDecl = cast<VarDecl> (d);
          // name: myType
          // type: struct MyType (or a loc identifier)
      llvm::outs() << "found VarDecl " << varDecl->getNameAsString() 
      << " type: " << varDecl->getType().getAsString() << "\n";

    } else if (isa<EnumDecl> (d)) {
      EnumDecl *enumDecl = cast<EnumDecl> (d);
      llvm::outs() << "found EnumDecl " << enumDecl->getNameAsString()+ "\n";
    } else {
      llvm::outs() << "found other declaration \n";
      d->dump();
    }
      // comment out because function declaration doesn't need to be traversed.
      // RecursiveASTVisitor<TraverseNodeVisitor>::TraverseDecl(d); // Forward to base class
      return true; // Return false to stop the AST analyzing
  }


  bool TraverseStmt(Stmt *x) {
    llvm::outs() << "found statement \n";
    x->dump();
    RecursiveASTVisitor<TraverseNodeVisitor>::TraverseStmt(x);
    return true;
  }
  bool TraverseType(QualType x) {
    llvm::outs() << "found type " << x.getAsString() << "\n";
    x->dump();
    RecursiveASTVisitor<TraverseNodeVisitor>::TraverseType(x);
    return true;
  }

private:
  Rewriter &TheRewriter;
};

class TraverseNodeConsumer : public clang::ASTConsumer {
public:
  TraverseNodeConsumer(Rewriter &R) : Visitor(R) {}

  virtual void HandleTranslationUnit(clang::ASTContext &Context) {
// Traversing the translation unit decl via a RecursiveASTVisitor
// will visit all nodes in the AST.
    llvm::outs() << "start traversing first declaration \n";
    Visitor.TraverseDecl(Context.getTranslationUnitDecl());
    llvm::outs() << "finished traversing last declaration \n";
  }

private:
// A RecursiveASTVisitor implementation.
  TraverseNodeVisitor Visitor;
};

class TraverseNodeAction : public clang::ASTFrontendAction {
public:
  TraverseNodeAction() {}
  // void EndSourceFileAction() override {
  //   Now emit the rewritten buffer.
  //   TheRewriter.getEditBuffer(TheRewriter.getSourceMgr().getMainFileID()).write(llvm::outs());
  // }

  std::unique_ptr<clang::ASTConsumer> CreateASTConsumer(
    clang::CompilerInstance &Compiler, llvm::StringRef InFile) override {
    TheRewriter.setSourceMgr(Compiler.getSourceManager(), Compiler.getLangOpts());
    return llvm::make_unique<TraverseNodeConsumer>(TheRewriter);
  }

private:
  Rewriter TheRewriter;
};

int main(int argc, const char **argv) {
  if (argc > 1) {
    CommonOptionsParser OptionsParser(argc, argv, MyToolCategory);
    ClangTool Tool(OptionsParser.getCompilations(), OptionsParser.getSourcePathList());
    return Tool.run(newFrontendActionFactory<TraverseNodeAction>().get());
  }
  //  else if (argc == 3) {
  //   clang::tooling::runToolOnCode(new TraverseNodeAction, argv[1]);
  // } else {
  //   llvm::outs() 
  //   << "USAGE: ~/clang-llvm/build/bin/node-inspect <PATH> OR "
  //   << "~/clang-llvm/build/bin/node-inspect <CODE> inline";
  // }
}