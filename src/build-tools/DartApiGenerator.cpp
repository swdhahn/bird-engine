// =============================================================================
// DartApiGenerator.cpp
// -----------------------------------------------------------------------------
// Parses curated C++ Bird Engine headers with libclang and emits a three-tier
// interop layer for embedded Dart scripting:
//
//   Tier 1: Flat C wrapper (extern "C") with exported symbols and docs.
//           Files:  <out-c>/c_wrapper.h, <out-c>/c_wrapper.cpp
//
//   Tier 2: Low-level Dart FFI bindings (dart:ffi) using
//           DynamicLibrary.process() (in-process embedded VM) with fallback.
//           File:   <out-dart>/bindings.dart
//
//   Tier 3: Idiomatic high-level Dart API mirroring the C++ classes,
//           inheritance, math operators, enums, free functions, singletons,
//           input constants, ownership semantics, and full Dartdoc comments
//           extracted from C++ Doxygen comments.
//
// Usage:
//   DartApiGenerator --out-c <dir> --out-dart <dir> [--lib <name>]
//                    [-- <clang args...>] <header>...
// =============================================================================

#include <clang-c/Index.h>

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <optional>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace fs = std::filesystem;

// =============================================================================
// 0. Utilities
// =============================================================================

static std::string toStr(CXString s) {
	const char* c = clang_getCString(s);
	std::string out = c ? c : "";
	clang_disposeString(s);
	return out;
}

static std::string cursorSpelling(CXCursor c) {
	return toStr(clang_getCursorSpelling(c));
}

static std::string typeSpelling(CXType t) {
	return toStr(clang_getTypeSpelling(t));
}

static std::string trim(std::string s) {
	auto sp = [](unsigned char ch) { return std::isspace(ch); };
	while (!s.empty() && sp(s.front())) s.erase(s.begin());
	while (!s.empty() && sp(s.back())) s.pop_back();
	return s;
}

static std::string replaceAll(std::string s, const std::string& a,
							  const std::string& b) {
	if (a.empty()) return s;
	for (size_t p = 0; (p = s.find(a, p)) != std::string::npos; p += b.size())
		s.replace(p, a.size(), b);
	return s;
}

static bool startsWith(const std::string& s, const std::string& p) {
	return s.size() >= p.size() && std::equal(p.begin(), p.end(), s.begin());
}

static bool endsWith(const std::string& s, const std::string& suf) {
	return s.size() >= suf.size() &&
		   std::equal(suf.rbegin(), suf.rend(), s.rbegin());
}

static std::string flattenNs(std::string s) {
	return replaceAll(std::move(s), "::", "_");
}

static std::string lastIdent(const std::string& qual) {
	auto p = qual.rfind("::");
	return p == std::string::npos ? qual : qual.substr(p + 2);
}

static std::string toSnakeCase(const std::string& s) {
	std::string out;
	for (size_t i = 0; i < s.size(); ++i) {
		char c = s[i];
		if (std::isupper(static_cast<unsigned char>(c))) {
			if (i > 0 &&
				(std::islower(static_cast<unsigned char>(s[i - 1])) ||
				 (i + 1 < s.size() &&
				  std::islower(static_cast<unsigned char>(s[i + 1]))))) {
				out += '_';
			}
			out += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
		} else {
			out += c;
		}
	}
	return out;
}

static std::string dartClassName(const std::string& simpleName) {
	// Avoid shadowing dart:core's Object class.
	if (simpleName == "Object") return "BirdObject";
	return simpleName;
}

static std::string safeDartIdent(std::string s) {
	static const std::set<std::string> kw = {
		"abstract",	 "else",	   "import",	"show",	   "as",
		"enum",		 "in",		   "static",	"assert",  "export",
		"interface", "super",	   "async",		"extends", "is",
		"switch",	 "await",	   "extension", "late",	   "sync",
		"break",	 "external",   "library",	"this",	   "case",
		"factory",	 "mixin",	   "throw",		"catch",   "false",
		"new",		 "true",	   "class",		"final",   "null",
		"try",		 "const",	   "finally",	"on",	   "typedef",
		"continue",	 "for",		   "operator",	"var",	   "covariant",
		"Function",	 "part",	   "void",		"default", "get",
		"required",	 "while",	   "deferred",	"hide",	   "rethrow",
		"with",		 "do",		   "if",		"return",  "yield",
		"dynamic",	 "implements", "set",		"Object"};
	if (kw.count(s)) return s + "_";
	if (s.empty()) return "arg";
	return s;
}

static std::string fullyQualifiedName(CXCursor c) {
	std::vector<std::string> parts;
	CXCursor cur = c;
	while (!clang_Cursor_isNull(cur)) {
		CXCursorKind k = clang_getCursorKind(cur);
		if (k == CXCursor_TranslationUnit) break;
		std::string name = cursorSpelling(cur);
		if (!name.empty()) parts.push_back(name);
		CXCursor parent = clang_getCursorSemanticParent(cur);
		if (clang_Cursor_isNull(parent) || clang_equalCursors(parent, cur))
			break;
		cur = parent;
	}
	std::reverse(parts.begin(), parts.end());
	std::string out;
	for (size_t i = 0; i < parts.size(); ++i) {
		if (i) out += "::";
		out += parts[i];
	}
	return out;
}

static bool hasNoExportAnnotation(CXCursor c) {
	bool flagged = false;
	clang_visitChildren(
		c,
		[](CXCursor child, CXCursor, CXClientData data) {
			if (clang_getCursorKind(child) == CXCursor_AnnotateAttr) {
				if (cursorSpelling(child) == "bird_no_export") {
					*static_cast<bool*>(data) = true;
					return CXChildVisit_Break;
				}
			}
			return CXChildVisit_Continue;
		},
		&flagged);
	return flagged;
}

static void warn(const std::string& msg) {
	std::cerr << "[DartApiGenerator] warn: " << msg << "\n";
}

// =============================================================================
// 1. Doxygen Comment Parsing -> C & Dartdoc Formatting
// =============================================================================

struct DocComment {
	std::vector<std::string> lines;	 // Cleaned markdown/prose lines

	bool empty() const { return lines.empty(); }
};

static DocComment extractDocComment(CXCursor c) {
	DocComment doc;
	std::string raw = toStr(clang_Cursor_getRawCommentText(c));
	if (raw.empty()) return doc;

	// Strip outer /* ... */ or /** ... */
	std::string s = trim(raw);
	if (startsWith(s, "/**")) {
		s = s.substr(3);
		if (endsWith(s, "*/")) s = s.substr(0, s.size() - 2);
	} else if (startsWith(s, "/*")) {
		s = s.substr(2);
		if (endsWith(s, "*/")) s = s.substr(0, s.size() - 2);
	}

	std::istringstream iss(s);
	std::string line;
	std::vector<std::string> rawLines;
	while (std::getline(iss, line)) {
		std::string t = trim(line);
		if (startsWith(t, "///")) {
			t = t.substr(3);
			if (!t.empty() && t[0] == ' ') t = t.substr(1);
		} else if (startsWith(t, "//!")) {
			t = t.substr(3);
			if (!t.empty() && t[0] == ' ') t = t.substr(1);
		} else if (startsWith(t, "*")) {
			t = t.substr(1);
			if (!t.empty() && t[0] == ' ') t = t.substr(1);
		}
		rawLines.push_back(t);
	}

	// Trim leading & trailing empty lines
	while (!rawLines.empty() && rawLines.front().empty())
		rawLines.erase(rawLines.begin());
	while (!rawLines.empty() && rawLines.back().empty()) rawLines.pop_back();

	// Convert Doxygen tags (@param, @return, @throws, @tparam) to Dartdoc
	for (const auto& l : rawLines) {
		if (startsWith(l, "@param ")) {
			std::string rest = trim(l.substr(7));
			size_t sp = rest.find_first_of(" \t");
			if (sp != std::string::npos) {
				std::string pname = rest.substr(0, sp);
				std::string desc = trim(rest.substr(sp + 1));
				doc.lines.push_back("- [" + pname + "]: " + desc);
			} else {
				doc.lines.push_back("- [" + rest + "]");
			}
		} else if (startsWith(l, "@tparam ")) {
			std::string rest = trim(l.substr(8));
			doc.lines.push_back("Type parameter: " + rest);
		} else if (startsWith(l, "@return ")) {
			doc.lines.push_back("Returns " + trim(l.substr(8)));
		} else if (startsWith(l, "@returns ")) {
			doc.lines.push_back("Returns " + trim(l.substr(9)));
		} else if (startsWith(l, "@throws ")) {
			std::string rest = trim(l.substr(8));
			size_t sp = rest.find_first_of(" \t");
			if (sp != std::string::npos) {
				std::string ex = rest.substr(0, sp);
				std::string desc = trim(rest.substr(sp + 1));
				doc.lines.push_back("Throws `" + ex + "` " + desc);
			} else {
				doc.lines.push_back("Throws `" + rest + "`");
			}
		} else {
			doc.lines.push_back(l);
		}
	}
	return doc;
}

static void emitDartDoc(std::ostream& out, const DocComment& doc,
						const std::string& indent) {
	if (doc.empty()) return;
	for (const auto& l : doc.lines) {
		if (l.empty())
			out << indent << "///\n";
		else
			out << indent << "/// " << l << "\n";
	}
}

static void emitCDoc(std::ostream& out, const DocComment& doc,
					 const std::string& indent = "") {
	if (doc.empty()) return;
	if (doc.lines.size() == 1) {
		out << indent << "/** " << doc.lines[0] << " */\n";
		return;
	}
	out << indent << "/**\n";
	for (const auto& l : doc.lines) {
		if (l.empty())
			out << indent << " *\n";
		else
			out << indent << " * " << l << "\n";
	}
	out << indent << " */\n";
}

// =============================================================================
// 2. Type System
// =============================================================================

enum class TypeCat {
	Void,
	Bool,
	Int32,
	UInt32,
	Int64,
	UInt64,
	Float,
	Double,
	CString,	   // const char*
	StdString,	   // std::string / const std::string&
	GlmVec2,
	GlmVec3,
	GlmVec4,
	GlmQuat,
	GlmMat3,
	GlmMat4,
	Enum,		   // enum / enum class
	OpaqueClass,   // user class T, T*, T&, const T&
	UniquePtr,	   // std::unique_ptr<T> (_own<T>)
	SharedPtr,	   // std::shared_ptr<T> (_ref<T>)
	VectorInt,	   // std::vector<int>
	VectorString,  // std::vector<std::string>
	VectorOpaque,  // std::vector<T*>, std::vector<_own<T>>, or std::vector<T>
	VectorShared,  // std::vector<_ref<T>>
	StdFunction,   // std::function<void(Args...)>
	Unsupported
};

struct TypeInfo {
	TypeCat cat = TypeCat::Unsupported;
	std::string cppRaw;
	std::string cppCanonical;  // e.g. "bird::Scene" or enum/inner class name
	bool isConst = false;
	bool isPointer = false;
	bool isReference = false;
	bool innerIsUniquePtr = false;	// for std::vector<_own<T>>
	bool innerIsPointer = false;	// for std::vector<T*>
	bool innerIsValue = false;		// for std::vector<T> (value structs)
	std::vector<TypeInfo> fnParams;	// for std::function<void(Args...)>
};

static std::string stripCvPtrRef(std::string s) {
	s = trim(s);
	while (startsWith(s, "const ")) {
		s = trim(s.substr(6));
	}
	while (startsWith(s, "class ")) {
		s = trim(s.substr(6));
	}
	while (startsWith(s, "struct ")) {
		s = trim(s.substr(7));
	}
	while (startsWith(s, "enum ")) {
		s = trim(s.substr(5));
	}
	while (!s.empty() && (s.back() == '*' || s.back() == '&')) {
		s.pop_back();
		s = trim(s);
		if (endsWith(s, " const")) s = trim(s.substr(0, s.size() - 6));
	}
	return s;
}

static bool isStdStringName(const std::string& s) {
	return s == "std::string" ||
		   s == "std::__cxx11::basic_string<char>" ||
		   startsWith(s, "std::basic_string<char") ||
		   startsWith(s, "std::__cxx11::basic_string<char") ||
		   startsWith(s, "std::__1::basic_string<char");
}

static std::string getRecordQualifiedName(CXType t) {
	CXCursor decl = clang_getTypeDeclaration(t);
	if (!clang_Cursor_isNull(decl) &&
		clang_getCursorKind(decl) != CXCursor_NoDeclFound) {
		std::string qn = fullyQualifiedName(decl);
		if (!qn.empty()) return qn;
	}
	return stripCvPtrRef(typeSpelling(t));
}

static TypeInfo translateType(CXType t) {
	TypeInfo info;
	info.cppRaw = typeSpelling(t);
	info.isConst = clang_isConstQualifiedType(t);

	CXType inner = t;
	if (t.kind == CXType_Pointer) {
		info.isPointer = true;
		inner = clang_getPointeeType(t);
		info.isConst = info.isConst || clang_isConstQualifiedType(inner);
	} else if (t.kind == CXType_LValueReference ||
			   t.kind == CXType_RValueReference) {
		info.isReference = true;
		inner = clang_getPointeeType(t);
		info.isConst = info.isConst || clang_isConstQualifiedType(inner);
	}

	CXType canon = clang_getCanonicalType(inner);
	std::string canonStr = stripCvPtrRef(typeSpelling(canon));
	info.cppCanonical = canonStr;

	// const char* -> CString
	if (info.isPointer &&
		(canon.kind == CXType_Char_S || canon.kind == CXType_SChar ||
		 canonStr == "char" || canonStr == "signed char")) {
		info.cat = TypeCat::CString;
		return info;
	}

	// std::string (by value or const ref)
	if (!info.isPointer && isStdStringName(canonStr)) {
		info.cat = TypeCat::StdString;
		info.cppCanonical = "std::string";
		return info;
	}

	// Primitives: allow by value OR const reference (e.g. const float&, const uint64_t&)
	if (!info.isPointer && (!info.isReference || info.isConst)) {
		switch (canon.kind) {
			case CXType_Void:
				if (!info.isReference) {
					info.cat = TypeCat::Void;
					return info;
				}
				break;
			case CXType_Bool:
				info.cat = TypeCat::Bool;
				return info;
			case CXType_Int:
			case CXType_Short:
			case CXType_SChar:
			case CXType_Char_S:
				info.cat = TypeCat::Int32;
				return info;
			case CXType_UInt:
			case CXType_UShort:
			case CXType_UChar:
			case CXType_Char_U:
				info.cat = TypeCat::UInt32;
				return info;
			case CXType_Long:
			case CXType_LongLong:
				info.cat = TypeCat::Int64;
				return info;
			case CXType_ULong:
			case CXType_ULongLong:
				info.cat = TypeCat::UInt64;
				return info;
			case CXType_Float:
				info.cat = TypeCat::Float;
				return info;
			case CXType_Double:
			case CXType_LongDouble:
				info.cat = TypeCat::Double;
				return info;
			case CXType_Enum:
				info.cat = TypeCat::Enum;
				info.cppCanonical = getRecordQualifiedName(canon);
				return info;
			default:
				break;
		}
	}

	// GLM types (handle both vec3 and glm::vec<3, float, ...>)
	if (!info.isPointer) {
		if (canonStr == "glm::vec2" || startsWith(canonStr, "glm::vec<2, float")) {
			info.cat = TypeCat::GlmVec2;
			return info;
		}
		if (canonStr == "glm::vec3" || startsWith(canonStr, "glm::vec<3, float")) {
			info.cat = TypeCat::GlmVec3;
			return info;
		}
		if (canonStr == "glm::vec4" || startsWith(canonStr, "glm::vec<4, float")) {
			info.cat = TypeCat::GlmVec4;
			return info;
		}
		if (canonStr == "glm::quat" || startsWith(canonStr, "glm::qua<float")) {
			info.cat = TypeCat::GlmQuat;
			return info;
		}
		if (canonStr == "glm::mat3" || canonStr == "glm::mat3x3" ||
			startsWith(canonStr, "glm::mat<3, 3, float")) {
			info.cat = TypeCat::GlmMat3;
			return info;
		}
		if (canonStr == "glm::mat4" || canonStr == "glm::mat4x4" ||
			startsWith(canonStr, "glm::mat<4, 4, float")) {
			info.cat = TypeCat::GlmMat4;
			return info;
		}
	}

	// Smart pointers and std::vector via template arguments
	if (canon.kind == CXType_Record) {
		std::string recName = getRecordQualifiedName(canon);

		// std::unique_ptr<T> (_own<T>)
		if (!info.isPointer &&
			(recName == "std::unique_ptr" || recName == "std::__1::unique_ptr")) {
			int nArgs = clang_Type_getNumTemplateArguments(canon);
			if (nArgs >= 1) {
				CXType elem = clang_Type_getTemplateArgumentAsType(canon, 0);
				CXType elemCanon = clang_getCanonicalType(elem);
				if (elemCanon.kind == CXType_Record) {
					info.cat = TypeCat::UniquePtr;
					info.cppCanonical = getRecordQualifiedName(elemCanon);
					return info;
				}
			}
			info.cat = TypeCat::Unsupported;
			return info;
		}

		// std::shared_ptr<T> (_ref<T>)
		if (!info.isPointer &&
			(recName == "std::shared_ptr" || recName == "std::__1::shared_ptr")) {
			int nArgs = clang_Type_getNumTemplateArguments(canon);
			if (nArgs >= 1) {
				CXType elem = clang_Type_getTemplateArgumentAsType(canon, 0);
				CXType elemCanon = clang_getCanonicalType(elem);
				if (elemCanon.kind == CXType_Record) {
					info.cat = TypeCat::SharedPtr;
					info.cppCanonical = getRecordQualifiedName(elemCanon);
					return info;
				}
			}
			info.cat = TypeCat::Unsupported;
			return info;
		}

		// std::vector<T>
		if (!info.isPointer &&
			(recName == "std::vector" || recName == "std::__1::vector")) {
			int nArgs = clang_Type_getNumTemplateArguments(canon);
			if (nArgs >= 1) {
				CXType elem = clang_Type_getTemplateArgumentAsType(canon, 0);
				CXType elemCanon = clang_getCanonicalType(elem);
				if (elemCanon.kind == CXType_Int) {
					info.cat = TypeCat::VectorInt;
					info.cppCanonical = "int";
					return info;
				}
				if (elemCanon.kind == CXType_Pointer) {
					CXType pt = clang_getCanonicalType(clang_getPointeeType(elemCanon));
					if (pt.kind == CXType_Record) {
						info.cat = TypeCat::VectorOpaque;
						info.cppCanonical = getRecordQualifiedName(pt);
						info.innerIsPointer = true;
						return info;
					}
				}
				if (elemCanon.kind == CXType_Record) {
					std::string eRec = getRecordQualifiedName(elemCanon);
					if (eRec == "std::unique_ptr" || eRec == "std::__1::unique_ptr") {
						CXType innerT = clang_getCanonicalType(
							clang_Type_getTemplateArgumentAsType(elemCanon, 0));
						if (innerT.kind == CXType_Record) {
							info.cat = TypeCat::VectorOpaque;
							info.cppCanonical = getRecordQualifiedName(innerT);
							info.innerIsUniquePtr = true;
							return info;
						}
					}
					if (eRec == "std::shared_ptr" || eRec == "std::__1::shared_ptr") {
						CXType innerT = clang_getCanonicalType(
							clang_Type_getTemplateArgumentAsType(elemCanon, 0));
						if (innerT.kind == CXType_Record) {
							info.cat = TypeCat::VectorShared;
							info.cppCanonical = getRecordQualifiedName(innerT);
							return info;
						}
					}
					std::string eCanonStr = stripCvPtrRef(typeSpelling(elemCanon));
					if (isStdStringName(eCanonStr)) {
						info.cat = TypeCat::VectorString;
						info.cppCanonical = "std::string";
						return info;
					}
					if (startsWith(eRec, "bird::")) {
						info.cat = TypeCat::VectorOpaque;
						info.cppCanonical = eRec;
						info.innerIsValue = true;
						return info;
					}
				}
			}
			info.cat = TypeCat::Unsupported;
			return info;
		}

		// std::function<void(Args...)>
		if (!info.isPointer &&
			(recName == "std::function" || recName == "std::__1::function")) {
			int nArgs = clang_Type_getNumTemplateArguments(canon);
			if (nArgs >= 1) {
				CXType fnType = clang_getCanonicalType(
					clang_Type_getTemplateArgumentAsType(canon, 0));
				if (fnType.kind == CXType_FunctionProto) {
					CXType retT = clang_getCanonicalType(clang_getResultType(fnType));
					if (retT.kind == CXType_Void) {
						int nFnParams = clang_getNumArgTypes(fnType);
						bool allOk = true;
						std::vector<TypeInfo> fparams;
						for (int i = 0; i < nFnParams; ++i) {
							TypeInfo pt = translateType(clang_getArgType(fnType, i));
							switch (pt.cat) {
								case TypeCat::Bool:
								case TypeCat::Int32:
								case TypeCat::UInt32:
								case TypeCat::Int64:
								case TypeCat::UInt64:
								case TypeCat::Float:
								case TypeCat::Double:
								case TypeCat::CString:
								case TypeCat::StdString:
								case TypeCat::GlmVec2:
								case TypeCat::GlmVec3:
								case TypeCat::GlmVec4:
								case TypeCat::GlmQuat:
								case TypeCat::Enum:
									fparams.push_back(std::move(pt));
									break;
								default:
									allOk = false;
									break;
							}
						}
						if (allOk) {
							info.cat = TypeCat::StdFunction;
							info.fnParams = std::move(fparams);
							return info;
						}
					}
				}
			}
			info.cat = TypeCat::Unsupported;
			return info;
		}

		// Regular user-defined class/struct
		info.cat = TypeCat::OpaqueClass;
		info.cppCanonical = recName;
		return info;
	}

	info.cat = TypeCat::Unsupported;
	return info;
}

// =============================================================================
// 3. Intermediate Representation
// =============================================================================

struct EnumValueInfo {
	std::string name;
	int64_t value = 0;
	DocComment doc;
};

struct EnumInfo {
	std::string qualifiedName;
	std::string flatName;
	std::string simpleName;
	bool isScoped = false;
	DocComment doc;
	std::vector<EnumValueInfo> values;
};

struct ParamInfo {
	std::string name;
	TypeInfo type;
	std::string defaultExpr;
};

struct MethodInfo {
	std::string name;
	std::string mangledSuffix;
	bool isStatic = false;
	bool isVirtual = false;
	bool isPureVirtual = false;
	bool isConst = false;
	bool isConstructor = false;
	bool isDestructor = false;
	bool isOverride = false;
	TypeInfo retType;
	std::vector<ParamInfo> params;
	DocComment doc;
};

struct FieldInfo {
	std::string name;
	TypeInfo type;
	bool isStatic = false;
	bool isReadOnly = false;
	DocComment doc;
};

struct ClassInfo {
	std::string qualifiedName;	// e.g. "bird::Scene"
	std::string flatName;		// e.g. "bird_Scene"
	std::string simpleName;		// e.g. "Scene"
	std::string dartName;		// e.g. "Scene" or "BirdObject"
	std::string baseQualified;	// e.g. "bird::WorldObject" (empty if none)
	DocComment doc;
	std::vector<MethodInfo> methods;
	std::vector<FieldInfo> fields;
	bool isAbstract = false;
	bool hasUserConstructor = false;
	bool hasPublicDestructor = true;
	bool isDefaultConstructible = false;
};

struct FreeFuncInfo {
	std::string qualifiedName;
	std::string simpleName;
	std::string mangledSuffix;
	TypeInfo retType;
	std::vector<ParamInfo> params;
	DocComment doc;
};

struct GlobalSingletonInfo {
	std::string qualifiedName;	// e.g. "bird::INPUT"
	std::string simpleName;		// e.g. "INPUT"
	std::string dartName;		// e.g. "input"
	std::string targetClass;	// e.g. "bird::Input"
	DocComment doc;
};

struct MacroConstInfo {
	std::string name;
	int64_t value = 0;
};

struct SharedRefType {
	std::string qualifiedName;	// e.g. "bird::Mesh"
	std::string flatName;		// e.g. "bird_Mesh"
	std::string dartName;		// e.g. "MeshRef"
};

struct Model {
	std::map<std::string, ClassInfo> classes;
	std::map<std::string, EnumInfo> enums;
	std::vector<FreeFuncInfo> freeFunctions;
	std::map<std::string, GlobalSingletonInfo> singletons;
	std::vector<MacroConstInfo> macroConstants;
	std::map<std::string, SharedRefType> sharedRefTypes;
	std::set<std::string> targetHeaderCanonicalPaths;
};

// =============================================================================
// 4. AST Visitor
// =============================================================================

struct VisitorState {
	Model* model = nullptr;
	std::set<std::string> nsFilter;
};

static bool inScope(const std::string& qualifiedName,
					const std::set<std::string>& nsFilter) {
	if (nsFilter.empty()) return true;
	for (const auto& ns : nsFilter) {
		if (startsWith(qualifiedName, ns + "::")) return true;
	}
	return false;
}

static bool cursorInTargetHeaders(CXCursor c, const Model& model) {
	CXSourceLocation loc = clang_getCursorLocation(c);
	if (clang_Location_isInSystemHeader(loc)) return false;
	CXFile file = nullptr;
	unsigned line = 0, col = 0, off = 0;
	clang_getSpellingLocation(loc, &file, &line, &col, &off);
	if (!file) return false;
	std::string path = toStr(clang_getFileName(file));
	if (path.empty()) return false;
	std::error_code ec;
	auto canon = fs::weakly_canonical(fs::path(path), ec).string();
	return model.targetHeaderCanonicalPaths.count(canon) > 0;
}

static std::string extractDefaultExpr(CXCursor paramCursor) {
	std::string out;
	clang_visitChildren(
		paramCursor,
		[](CXCursor c, CXCursor, CXClientData data) {
			CXCursorKind k = clang_getCursorKind(c);
			if (clang_isExpression(k)) {
				CXSourceRange r = clang_getCursorExtent(c);
				CXTranslationUnit tu = clang_Cursor_getTranslationUnit(c);
				CXToken* tokens = nullptr;
				unsigned n = 0;
				clang_tokenize(tu, r, &tokens, &n);
				std::string acc;
				for (unsigned i = 0; i < n; ++i) {
					if (i) acc += " ";
					acc += toStr(clang_getTokenSpelling(tu, tokens[i]));
				}
				if (tokens) clang_disposeTokens(tu, tokens, n);
				*static_cast<std::string*>(data) = trim(acc);
				return CXChildVisit_Break;
			}
			return CXChildVisit_Continue;
		},
		&out);
	return out;
}

static MethodInfo buildMethod(CXCursor cur) {
	MethodInfo m;
	m.name = cursorSpelling(cur);
	m.isStatic = clang_CXXMethod_isStatic(cur);
	m.isVirtual = clang_CXXMethod_isVirtual(cur);
	m.isPureVirtual = clang_CXXMethod_isPureVirtual(cur);
	m.isConst = clang_CXXMethod_isConst(cur);
	m.doc = extractDocComment(cur);

	CXCursorKind kind = clang_getCursorKind(cur);
	m.isConstructor = (kind == CXCursor_Constructor);
	m.isDestructor = (kind == CXCursor_Destructor);

	if (!m.isConstructor && !m.isDestructor) {
		m.retType =
			translateType(clang_getResultType(clang_getCursorType(cur)));
	} else {
		m.retType.cat = TypeCat::Void;
	}

	int n = clang_Cursor_getNumArguments(cur);
	for (int i = 0; i < n; ++i) {
		CXCursor pc = clang_Cursor_getArgument(cur, i);
		ParamInfo p;
		p.name = cursorSpelling(pc);
		if (p.name.empty()) p.name = "arg" + std::to_string(i);
		p.type = translateType(clang_getCursorType(pc));
		p.defaultExpr = extractDefaultExpr(pc);
		m.params.push_back(std::move(p));
	}
	return m;
}

static CXChildVisitResult visitClassBody(CXCursor c, CXCursor /*parent*/,
										 CXClientData data) {
	auto* cls = static_cast<ClassInfo*>(data);
	CXCursorKind kind = clang_getCursorKind(c);

	if (kind == CXCursor_CXXBaseSpecifier) {
		if (clang_getCXXAccessSpecifier(c) == CX_CXXPublic &&
			cls->baseQualified.empty()) {
			CXType baseType = clang_getCanonicalType(clang_getCursorType(c));
			cls->baseQualified = getRecordQualifiedName(baseType);
		}
		return CXChildVisit_Continue;
	}

	CX_CXXAccessSpecifier acc = clang_getCXXAccessSpecifier(c);
	if (kind == CXCursor_Destructor) {
		if (acc != CX_CXXPublic) cls->hasPublicDestructor = false;
		return CXChildVisit_Continue;
	}

	if (acc == CX_CXXPrivate || acc == CX_CXXProtected)
		return CXChildVisit_Continue;

	if (hasNoExportAnnotation(c)) return CXChildVisit_Continue;

	switch (kind) {
		case CXCursor_CXXMethod:
		case CXCursor_Constructor: {
			// Skip copy/move constructors and operators.
			if (kind == CXCursor_Constructor &&
				(clang_CXXConstructor_isCopyConstructor(c) ||
				 clang_CXXConstructor_isMoveConstructor(c))) {
				cls->hasUserConstructor = true;
				return CXChildVisit_Continue;
			}
			MethodInfo m = buildMethod(c);
			if (m.isPureVirtual) {
				cls->isAbstract = true;
				return CXChildVisit_Continue;
			}
			if (startsWith(m.name, "operator")) return CXChildVisit_Continue;
			if (!m.isConstructor && !m.isDestructor && startsWith(m.name, "_"))
				return CXChildVisit_Continue;
			if (m.isConstructor) cls->hasUserConstructor = true;
			cls->methods.push_back(std::move(m));
			return CXChildVisit_Continue;
		}
		case CXCursor_FieldDecl: {
			FieldInfo f;
			f.name = cursorSpelling(c);
			f.type = translateType(clang_getCursorType(c));
			f.isStatic = false;
			f.doc = extractDocComment(c);
			if (f.type.cat != TypeCat::Unsupported)
				cls->fields.push_back(std::move(f));
			return CXChildVisit_Continue;
		}
		case CXCursor_VarDecl: {
			// Static public member variable or static constexpr constant
			// (e.g. ResourceManager::RESOURCE_PATH, UIAlignment::TopLeft, UIDropdown::kNone)
			std::string name = cursorSpelling(c);
			if (startsWith(name, "s_")) return CXChildVisit_Continue;
			CXType vt = clang_getCursorType(c);
			FieldInfo f;
			f.name = name;
			f.type = translateType(vt);
			f.isStatic = true;
			f.isReadOnly = clang_isConstQualifiedType(vt) != 0;
			f.doc = extractDocComment(c);
			if (f.type.cat != TypeCat::Unsupported)
				cls->fields.push_back(std::move(f));
			return CXChildVisit_Continue;
		}
		default:
			return CXChildVisit_Continue;
	}
}

static CXChildVisitResult visitEnumBody(CXCursor c, CXCursor /*parent*/,
										CXClientData data) {
	auto* en = static_cast<EnumInfo*>(data);
	if (clang_getCursorKind(c) == CXCursor_EnumConstantDecl) {
		EnumValueInfo ev;
		ev.name = cursorSpelling(c);
		ev.value = clang_getEnumConstantDeclValue(c);
		ev.doc = extractDocComment(c);
		en->values.push_back(std::move(ev));
	}
	return CXChildVisit_Continue;
}

static std::string toLowerCamelSingleton(const std::string& upper) {
	// INPUT -> input, RESOURCE_MANAGER -> resourceManager, APPLICATION -> application
	std::string out;
	bool nextCap = false;
	for (char c : upper) {
		if (c == '_') {
			nextCap = true;
			continue;
		}
		if (nextCap) {
			out += static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
			nextCap = false;
		} else {
			out += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
		}
	}
	return out;
}

static CXChildVisitResult visitTU(CXCursor c, CXCursor /*parent*/,
								  CXClientData data) {
	auto* st = static_cast<VisitorState*>(data);
	CXSourceLocation loc = clang_getCursorLocation(c);
	if (clang_Location_isInSystemHeader(loc)) return CXChildVisit_Continue;

	CXCursorKind kind = clang_getCursorKind(c);

	if (kind == CXCursor_Namespace) return CXChildVisit_Recurse;

	if (!cursorInTargetHeaders(c, *st->model)) return CXChildVisit_Continue;
	if (hasNoExportAnnotation(c)) return CXChildVisit_Continue;

	if (kind == CXCursor_ClassDecl || kind == CXCursor_StructDecl) {
		if (!clang_isCursorDefinition(c)) return CXChildVisit_Continue;
		std::string qn = fullyQualifiedName(c);
		if (!inScope(qn, st->nsFilter)) return CXChildVisit_Continue;

		if (st->model->classes.count(qn)) return CXChildVisit_Continue;

		ClassInfo cls;
		cls.qualifiedName = qn;
		cls.flatName = flattenNs(qn);
		cls.simpleName = lastIdent(qn);
		cls.dartName = dartClassName(cls.simpleName);
		cls.doc = extractDocComment(c);
		cls.isAbstract = clang_CXXRecord_isAbstract(c) != 0;

		clang_visitChildren(c, visitClassBody, &cls);

		if (!cls.isAbstract && !cls.hasUserConstructor) {
			cls.isDefaultConstructible = true;
		}
		st->model->classes.emplace(qn, std::move(cls));
		return CXChildVisit_Continue;
	}

	if (kind == CXCursor_EnumDecl) {
		if (!clang_isCursorDefinition(c)) return CXChildVisit_Continue;
		std::string qn = fullyQualifiedName(c);
		if (!inScope(qn, st->nsFilter)) return CXChildVisit_Continue;
		if (st->model->enums.count(qn)) return CXChildVisit_Continue;

		EnumInfo en;
		en.qualifiedName = qn;
		en.flatName = flattenNs(qn);
		en.simpleName = lastIdent(qn);
		en.isScoped = clang_EnumDecl_isScoped(c) != 0;
		en.doc = extractDocComment(c);
		clang_visitChildren(c, visitEnumBody, &en);
		if (!en.values.empty()) st->model->enums.emplace(qn, std::move(en));
		return CXChildVisit_Continue;
	}

	if (kind == CXCursor_FunctionDecl) {
		std::string qn = fullyQualifiedName(c);
		if (!inScope(qn, st->nsFilter)) return CXChildVisit_Continue;
		std::string simple = lastIdent(qn);
		if (startsWith(simple, "operator")) return CXChildVisit_Continue;

		FreeFuncInfo fn;
		fn.qualifiedName = qn;
		fn.simpleName = simple;
		fn.doc = extractDocComment(c);
		fn.retType =
			translateType(clang_getResultType(clang_getCursorType(c)));
		int n = clang_Cursor_getNumArguments(c);
		for (int i = 0; i < n; ++i) {
			CXCursor pc = clang_Cursor_getArgument(c, i);
			ParamInfo p;
			p.name = cursorSpelling(pc);
			if (p.name.empty()) p.name = "arg" + std::to_string(i);
			p.type = translateType(clang_getCursorType(pc));
			p.defaultExpr = extractDefaultExpr(pc);
			fn.params.push_back(std::move(p));
		}
		st->model->freeFunctions.push_back(std::move(fn));
		return CXChildVisit_Continue;
	}

	if (kind == CXCursor_VarDecl) {
		// Global extern singleton pointers in namespace bird:
		// e.g. extern BIRD_API Input* INPUT;
		std::string qn = fullyQualifiedName(c);
		if (!inScope(qn, st->nsFilter)) return CXChildVisit_Continue;
		CXType vt = clang_getCursorType(c);
		if (vt.kind == CXType_Pointer) {
			CXType pt = clang_getCanonicalType(clang_getPointeeType(vt));
			if (pt.kind == CXType_Record) {
				std::string targetCls = getRecordQualifiedName(pt);
				if (inScope(targetCls, st->nsFilter) &&
					!st->model->singletons.count(qn)) {
					GlobalSingletonInfo g;
					g.qualifiedName = qn;
					g.simpleName = lastIdent(qn);
					g.dartName = toLowerCamelSingleton(g.simpleName);
					g.targetClass = targetCls;
					g.doc = extractDocComment(c);
					st->model->singletons.emplace(qn, std::move(g));
				}
			}
		}
		return CXChildVisit_Continue;
	}

	return CXChildVisit_Continue;
}

// Extract integer #define constants (KEY_*, MOUSE_BUTTON_*, CURSOR_*, ACTION_*)
// directly from Input.h or any target header with those prefixes.
static void extractInputMacroConstants(const std::vector<fs::path>& headers,
									   Model& model) {
	std::map<std::string, int64_t> resolved;
	std::vector<std::string> order;
	std::regex defRe(
		R"(^\s*#\s*define\s+((?:KEY|MOUSE_BUTTON|CURSOR|ACTION)_[A-Z0-9_]+)\s+([^\s/*]+))");

	for (const auto& h : headers) {
		std::ifstream in(h);
		if (!in) continue;
		std::string line;
		while (std::getline(in, line)) {
			std::smatch m;
			if (std::regex_search(line, m, defRe)) {
				std::string name = m[1].str();
				std::string valStr = m[2].str();
				int64_t val = 0;
				bool ok = false;
				if (resolved.count(valStr)) {
					val = resolved[valStr];
					ok = true;
				} else {
					try {
						size_t idx = 0;
						val = std::stoll(valStr, &idx, 0);
						if (idx == valStr.size()) ok = true;
					} catch (...) {
						ok = false;
					}
				}
				if (ok && !resolved.count(name)) {
					resolved[name] = val;
					order.push_back(name);
				}
			}
		}
	}
	for (const auto& name : order) {
		model.macroConstants.push_back({name, resolved[name]});
	}
}

// =============================================================================
// 5. Post-Parse Validation, SharedRef Registration, Inheritance & Overloads
// =============================================================================

static bool isTypeExportable(const TypeInfo& t, Model& model, bool isReturn) {
	switch (t.cat) {
		case TypeCat::Void:
			return isReturn;
		case TypeCat::Bool:
		case TypeCat::Int32:
		case TypeCat::UInt32:
		case TypeCat::Int64:
		case TypeCat::UInt64:
		case TypeCat::Float:
		case TypeCat::Double:
		case TypeCat::CString:
		case TypeCat::StdString:
		case TypeCat::GlmVec2:
		case TypeCat::GlmVec3:
		case TypeCat::GlmVec4:
		case TypeCat::GlmQuat:
		case TypeCat::GlmMat3:
		case TypeCat::GlmMat4:
			return true;
		case TypeCat::Enum:
			return model.enums.count(t.cppCanonical) > 0;
		case TypeCat::OpaqueClass:
			return model.classes.count(t.cppCanonical) > 0;
		case TypeCat::UniquePtr:
			// Supported as parameter (ownership transfer into C++).
			return !isReturn && model.classes.count(t.cppCanonical) > 0;
		case TypeCat::SharedPtr: {
			if (!startsWith(t.cppCanonical, "bird::")) return false;
			if (!model.sharedRefTypes.count(t.cppCanonical)) {
				SharedRefType sr;
				sr.qualifiedName = t.cppCanonical;
				sr.flatName = flattenNs(t.cppCanonical);
				std::string simple = lastIdent(t.cppCanonical);
				sr.dartName = simple + "Ref";
				model.sharedRefTypes.emplace(t.cppCanonical, std::move(sr));
			}
			return true;
		}
		case TypeCat::VectorInt:
			return isReturn;
		case TypeCat::VectorString:
			return true;
		case TypeCat::VectorOpaque:
			if (!model.classes.count(t.cppCanonical)) return false;
			return isReturn || t.innerIsValue;
		case TypeCat::VectorShared: {
			if (!startsWith(t.cppCanonical, "bird::")) return false;
			if (!model.sharedRefTypes.count(t.cppCanonical)) {
				SharedRefType sr;
				sr.qualifiedName = t.cppCanonical;
				sr.flatName = flattenNs(t.cppCanonical);
				sr.dartName = lastIdent(t.cppCanonical) + "Ref";
				model.sharedRefTypes.emplace(t.cppCanonical, std::move(sr));
			}
			return true;
		}
		case TypeCat::StdFunction: {
			if (isReturn) return false;
			for (const auto& fp : t.fnParams) {
				if (fp.cat == TypeCat::Enum && !model.enums.count(fp.cppCanonical))
					return false;
			}
			return true;
		}
		case TypeCat::Unsupported:
		default:
			return false;
	}
}

static std::string paramTag(const TypeInfo& t) {
	switch (t.cat) {
		case TypeCat::Bool:
			return "Bool";
		case TypeCat::Int32:
			return "I32";
		case TypeCat::UInt32:
			return "U32";
		case TypeCat::Int64:
			return "I64";
		case TypeCat::UInt64:
			return "U64";
		case TypeCat::Float:
			return "F32";
		case TypeCat::Double:
			return "F64";
		case TypeCat::CString:
		case TypeCat::StdString:
			return "Str";
		case TypeCat::GlmVec2:
			return "Vec2";
		case TypeCat::GlmVec3:
			return "Vec3";
		case TypeCat::GlmVec4:
			return "Vec4";
		case TypeCat::GlmQuat:
			return "Quat";
		case TypeCat::GlmMat3:
			return "Mat3";
		case TypeCat::GlmMat4:
			return "Mat4";
		case TypeCat::Enum:
		case TypeCat::OpaqueClass:
		case TypeCat::UniquePtr:
			return lastIdent(t.cppCanonical);
		case TypeCat::SharedPtr:
			return lastIdent(t.cppCanonical) + "Ref";
		case TypeCat::VectorInt:
			return "IntList";
		case TypeCat::VectorString:
			return "StrList";
		case TypeCat::VectorOpaque:
			return lastIdent(t.cppCanonical) + "List";
		case TypeCat::VectorShared:
			return lastIdent(t.cppCanonical) + "RefList";
		case TypeCat::StdFunction:
			return "Callback";
		default:
			return "X";
	}
}

static bool baseHasMethod(const Model& model, const ClassInfo& cls,
						  const std::string& dartMethodName) {
	std::string base = cls.baseQualified;
	while (!base.empty()) {
		auto it = model.classes.find(base);
		if (it == model.classes.end()) break;
		for (const auto& m : it->second.methods) {
			if (!m.isConstructor && !m.isDestructor && !m.isStatic &&
				(m.name + m.mangledSuffix) == dartMethodName) {
				return true;
			}
		}
		base = it->second.baseQualified;
	}
	return false;
}

static void finalizeModel(Model& model) {
	// 1. Validate base classes: only keep bases that are also exported.
	for (auto& [_, cls] : model.classes) {
		if (!cls.baseQualified.empty() &&
			!model.classes.count(cls.baseQualified)) {
			cls.baseQualified.clear();
		}
	}

	// 2. Filter methods & fields whose signatures reference unsupported or
	// unexported types.
	for (auto& [_, cls] : model.classes) {
		std::vector<MethodInfo> validMethods;
		for (auto& m : cls.methods) {
			if (m.isConstructor && cls.isAbstract) continue;
			bool ok = m.isConstructor || isTypeExportable(m.retType, model, true);
			for (const auto& p : m.params) {
				ok = ok && isTypeExportable(p.type, model, false);
			}
			if (ok) {
				validMethods.push_back(std::move(m));
			} else {
				warn("skipping " + cls.qualifiedName + "::" + m.name +
					 " (unsupported or non-exported type)");
			}
		}
		cls.methods = std::move(validMethods);

		// Add synthetic default constructor for concrete default-constructible
		// classes with no explicit constructors (e.g. Light).
		if (cls.isDefaultConstructible && !cls.isAbstract) {
			MethodInfo defCtor;
			defCtor.name = cls.simpleName;
			defCtor.isConstructor = true;
			defCtor.retType.cat = TypeCat::Void;
			cls.methods.insert(cls.methods.begin(), std::move(defCtor));
		}

		// Filter fields
		std::vector<FieldInfo> validFields;
		for (auto& f : cls.fields) {
			bool getOk = isTypeExportable(f.type, model, true);
			bool setOk = f.isReadOnly || isTypeExportable(f.type, model, false);
			if (getOk && setOk) {
				validFields.push_back(std::move(f));
			}
		}
		cls.fields = std::move(validFields);

		// Disambiguate overloads within class
		std::map<std::string, int> counts;
		for (const auto& m : cls.methods) {
			std::string key = m.isConstructor ? "__ctor__" : m.name;
			counts[key]++;
		}
		for (auto& m : cls.methods) {
			std::string key = m.isConstructor ? "__ctor__" : m.name;
			if (counts[key] <= 1) continue;
			std::string s;
			for (const auto& p : m.params) {
				s += "_";
				s += paramTag(p.type);
			}
			if (s.empty()) s = "_void";
			m.mangledSuffix = s;
		}
	}

	// 3. Mark @override on methods matching a base class method name
	for (auto& [_, cls] : model.classes) {
		for (auto& m : cls.methods) {
			if (m.isConstructor || m.isDestructor || m.isStatic) continue;
			if (baseHasMethod(model, cls, m.name + m.mangledSuffix)) {
				m.isOverride = true;
			}
		}
	}

	// 4. Filter and disambiguate free functions
	std::vector<FreeFuncInfo> validFns;
	std::set<std::string> seenSigs;
	for (auto& fn : model.freeFunctions) {
		bool ok = isTypeExportable(fn.retType, model, true);
		for (const auto& p : fn.params) {
			ok = ok && isTypeExportable(p.type, model, false);
		}
		if (!ok) continue;
		std::string sigKey = fn.qualifiedName;
		for (const auto& p : fn.params) sigKey += "|" + paramTag(p.type);
		if (seenSigs.count(sigKey)) continue;
		seenSigs.insert(sigKey);
		validFns.push_back(std::move(fn));
	}
	model.freeFunctions = std::move(validFns);

	std::map<std::string, int> fnCounts;
	for (const auto& fn : model.freeFunctions) fnCounts[fn.simpleName]++;
	for (auto& fn : model.freeFunctions) {
		if (fnCounts[fn.simpleName] <= 1) continue;
		std::string s;
		for (const auto& p : fn.params) {
			s += "_";
			s += paramTag(p.type);
		}
		if (s.empty()) s = "_void";
		fn.mangledSuffix = s;
	}

	// 5. Filter singletons to exported target classes
	for (auto it = model.singletons.begin(); it != model.singletons.end();) {
		if (!model.classes.count(it->second.targetClass))
			it = model.singletons.erase(it);
		else
			++it;
	}
}

// =============================================================================
// 6. Type Emission Helpers
// =============================================================================

struct EmittedType {
	std::string cType;
	std::string ffiNative;
	std::string ffiDart;
	std::string dartUser;
	bool isOutPtr = false;	// Mat3 / Mat4 returned via out-ptr
};

static EmittedType emitType(const TypeInfo& t, const Model& model,
							bool isReturn) {
	EmittedType e;
	switch (t.cat) {
		case TypeCat::Void:
			e = {"void", "ffi.Void", "void", "void", false};
			return e;
		case TypeCat::Bool:
			e = {"bool", "ffi.Bool", "bool", "bool", false};
			return e;
		case TypeCat::Int32:
			e = {"int32_t", "ffi.Int32", "int", "int", false};
			return e;
		case TypeCat::UInt32:
			e = {"uint32_t", "ffi.Uint32", "int", "int", false};
			return e;
		case TypeCat::Int64:
			e = {"int64_t", "ffi.Int64", "int", "int", false};
			return e;
		case TypeCat::UInt64:
			e = {"uint64_t", "ffi.Uint64", "int", "int", false};
			return e;
		case TypeCat::Float:
			e = {"float", "ffi.Float", "double", "double", false};
			return e;
		case TypeCat::Double:
			e = {"double", "ffi.Double", "double", "double", false};
			return e;
		case TypeCat::CString:
			e = {"const char*", "ffi.Pointer<pkg_ffi.Utf8>",
				 "ffi.Pointer<pkg_ffi.Utf8>", "String", false};
			return e;
		case TypeCat::StdString:
			if (isReturn) {
				e = {"char*", "ffi.Pointer<pkg_ffi.Utf8>",
					 "ffi.Pointer<pkg_ffi.Utf8>", "String", false};
			} else {
				e = {"const char*", "ffi.Pointer<pkg_ffi.Utf8>",
					 "ffi.Pointer<pkg_ffi.Utf8>", "String", false};
			}
			return e;
		case TypeCat::GlmVec2:
			e = {"BVec2", "BVec2", "BVec2", "Vector2", false};
			return e;
		case TypeCat::GlmVec3:
			e = {"BVec3", "BVec3", "BVec3", "Vector3", false};
			return e;
		case TypeCat::GlmVec4:
			e = {"BVec4", "BVec4", "BVec4", "Vector4", false};
			return e;
		case TypeCat::GlmQuat:
			e = {"BQuat", "BQuat", "BQuat", "Quaternion", false};
			return e;
		case TypeCat::GlmMat3:
			if (isReturn) {
				e = {"void", "ffi.Void", "void", "Matrix3", true};
			} else {
				e = {"const BMat3*", "ffi.Pointer<BMat3>", "ffi.Pointer<BMat3>",
					 "Matrix3", false};
			}
			return e;
		case TypeCat::GlmMat4:
			if (isReturn) {
				e = {"void", "ffi.Void", "void", "Matrix4", true};
			} else {
				e = {"const BMat4*", "ffi.Pointer<BMat4>", "ffi.Pointer<BMat4>",
					 "Matrix4", false};
			}
			return e;
		case TypeCat::Enum: {
			std::string enName = model.enums.at(t.cppCanonical).simpleName;
			e = {"int32_t", "ffi.Int32", "int", enName, false};
			return e;
		}
		case TypeCat::OpaqueClass: {
			std::string dn = model.classes.at(t.cppCanonical).dartName;
			std::string userType = (isReturn && t.isPointer) ? (dn + "?") : dn;
			e = {"void*", "ffi.Pointer<ffi.Void>", "ffi.Pointer<ffi.Void>",
				 userType, false};
			return e;
		}
		case TypeCat::UniquePtr: {
			std::string dn = model.classes.at(t.cppCanonical).dartName;
			e = {"void*", "ffi.Pointer<ffi.Void>", "ffi.Pointer<ffi.Void>", dn,
				 false};
			return e;
		}
		case TypeCat::SharedPtr: {
			std::string dn = model.sharedRefTypes.at(t.cppCanonical).dartName;
			std::string userType = isReturn ? (dn + "?") : dn;
			e = {"void*", "ffi.Pointer<ffi.Void>", "ffi.Pointer<ffi.Void>",
				 userType, false};
			return e;
		}
		case TypeCat::VectorInt:
			e = {"BInt32List", "BInt32List", "BInt32List", "List<int>", false};
			return e;
		case TypeCat::VectorString:
			if (isReturn) {
				e = {"BStrList", "BStrList", "BStrList", "List<String>", false};
			} else {
				e = {"const char* const*",
					 "ffi.Pointer<ffi.Pointer<pkg_ffi.Utf8>>",
					 "ffi.Pointer<ffi.Pointer<pkg_ffi.Utf8>>", "List<String>",
					 false};
			}
			return e;
		case TypeCat::VectorOpaque: {
			std::string dn = model.classes.at(t.cppCanonical).dartName;
			if (isReturn) {
				e = {"BPtrList", "BPtrList", "BPtrList", "List<" + dn + ">",
					 false};
			} else {
				e = {"void* const*", "ffi.Pointer<ffi.Pointer<ffi.Void>>",
					 "ffi.Pointer<ffi.Pointer<ffi.Void>>", "List<" + dn + ">",
					 false};
			}
			return e;
		}
		case TypeCat::VectorShared: {
			std::string dn = model.sharedRefTypes.at(t.cppCanonical).dartName;
			if (isReturn) {
				e = {"BPtrList", "BPtrList", "BPtrList", "List<" + dn + ">",
					 false};
			} else {
				e = {"void* const*", "ffi.Pointer<ffi.Pointer<ffi.Void>>",
					 "ffi.Pointer<ffi.Pointer<ffi.Void>>", "List<" + dn + ">",
					 false};
			}
			return e;
		}
		case TypeCat::StdFunction: {
			std::string dartFn = "void Function(";
			for (size_t i = 0; i < t.fnParams.size(); ++i) {
				if (i) dartFn += ", ";
				dartFn += emitType(t.fnParams[i], model, false).dartUser;
			}
			dartFn += ")";
			e = {"void*", "ffi.Pointer<ffi.Void>", "ffi.Pointer<ffi.Void>",
				 dartFn, false};
			return e;
		}
		default:
			return e;
	}
}

// =============================================================================
// 7. Tier 1: C Wrapper Generation (c_wrapper.h, c_wrapper.cpp)
// =============================================================================

namespace cwrap {

static std::string cppArgExpr(const ParamInfo& p) {
	switch (p.type.cat) {
		case TypeCat::GlmVec2:
		case TypeCat::GlmVec3:
		case TypeCat::GlmVec4:
		case TypeCat::GlmQuat:
			return "to_glm(" + p.name + ")";
		case TypeCat::GlmMat3:
		case TypeCat::GlmMat4:
			return "to_glm(*" + p.name + ")";
		case TypeCat::StdString:
			return "std::string(" + p.name + " ? " + p.name + " : \"\")";
		case TypeCat::Enum:
			return "static_cast<::" + p.type.cppCanonical + ">(" + p.name + ")";
		case TypeCat::OpaqueClass: {
			std::string castExpr =
				"static_cast<::" + p.type.cppCanonical + "*>(" + p.name + ")";
			if (p.type.isPointer) return castExpr;
			return "(*" + castExpr + ")";
		}
		case TypeCat::UniquePtr:
			return "std::unique_ptr<::" + p.type.cppCanonical +
				   ">(static_cast<::" + p.type.cppCanonical + "*>(" + p.name +
				   "))";
		case TypeCat::SharedPtr:
			return "(*static_cast<std::shared_ptr<::" + p.type.cppCanonical +
				   ">*>(" + p.name + "))";
		case TypeCat::VectorShared:
		case TypeCat::VectorString:
		case TypeCat::VectorOpaque:
			return "_vec_" + p.name;
		case TypeCat::StdFunction:
			return "_cb_" + p.name;
		default:
			return p.name;
	}
}

static std::string cppCbParamDecl(const TypeInfo& t, size_t idx) {
	std::string an = "_a" + std::to_string(idx);
	switch (t.cat) {
		case TypeCat::StdString:
			return "const std::string& " + an;
		case TypeCat::GlmVec2:
			return "const glm::vec2& " + an;
		case TypeCat::GlmVec3:
			return "const glm::vec3& " + an;
		case TypeCat::GlmVec4:
			return "const glm::vec4& " + an;
		case TypeCat::GlmQuat:
			return "const glm::quat& " + an;
		case TypeCat::Enum:
			return "::" + t.cppCanonical + " " + an;
		case TypeCat::Bool:
			return "bool " + an;
		case TypeCat::Int32:
			return "int " + an;
		case TypeCat::UInt32:
			return "unsigned int " + an;
		case TypeCat::Int64:
			return "int64_t " + an;
		case TypeCat::UInt64:
			return "size_t " + an;
		case TypeCat::Float:
			return "float " + an;
		case TypeCat::Double:
			return "double " + an;
		default:
			return "int " + an;
	}
}

static std::string cppCbArgToC(const TypeInfo& t, size_t idx) {
	std::string an = "_a" + std::to_string(idx);
	switch (t.cat) {
		case TypeCat::StdString:
			return an + ".c_str()";
		case TypeCat::GlmVec2:
		case TypeCat::GlmVec3:
		case TypeCat::GlmVec4:
		case TypeCat::GlmQuat:
			return "from_glm(" + an + ")";
		case TypeCat::Enum:
			return "static_cast<int32_t>(" + an + ")";
		default:
			return an;
	}
}

static void emitVectorParamPrologue(std::ostream& src,
									const std::vector<ParamInfo>& params,
									const Model& model) {
	for (const auto& p : params) {
		if (p.type.cat == TypeCat::VectorShared) {
			src << "    std::vector<std::shared_ptr<::" << p.type.cppCanonical
				<< ">> _vec_" << p.name << ";\n";
			src << "    _vec_" << p.name << ".reserve(" << p.name
				<< "_count);\n";
			src << "    for (int32_t _i = 0; _i < " << p.name
				<< "_count; ++_i) {\n";
			src << "        _vec_" << p.name
				<< ".push_back(*static_cast<std::shared_ptr<::"
				<< p.type.cppCanonical << ">*>(" << p.name << "[_i]));\n";
			src << "    }\n";
		} else if (p.type.cat == TypeCat::VectorString) {
			src << "    std::vector<std::string> _vec_" << p.name << ";\n";
			src << "    _vec_" << p.name << ".reserve(" << p.name
				<< "_count);\n";
			src << "    for (int32_t _i = 0; _i < " << p.name
				<< "_count; ++_i) {\n";
			src << "        _vec_" << p.name << ".emplace_back(" << p.name
				<< "[_i] ? " << p.name << "[_i] : \"\");\n";
			src << "    }\n";
		} else if (p.type.cat == TypeCat::VectorOpaque && p.type.innerIsValue) {
			src << "    std::vector<::" << p.type.cppCanonical << "> _vec_"
				<< p.name << ";\n";
			src << "    _vec_" << p.name << ".reserve(" << p.name
				<< "_count);\n";
			src << "    for (int32_t _i = 0; _i < " << p.name
				<< "_count; ++_i) {\n";
			src << "        _vec_" << p.name
				<< ".push_back(*static_cast<const ::" << p.type.cppCanonical
				<< "*>(" << p.name << "[_i]));\n";
			src << "    }\n";
		} else if (p.type.cat == TypeCat::StdFunction) {
			std::string cSig = "void (*)(";
			std::string cppSig = "void(";
			std::string lambdaParams;
			std::string callArgs;
			for (size_t i = 0; i < p.type.fnParams.size(); ++i) {
				if (i) {
					cSig += ", ";
					cppSig += ", ";
					lambdaParams += ", ";
					callArgs += ", ";
				}
				cSig += emitType(p.type.fnParams[i], model, false).cType;
				std::string pd = cppCbParamDecl(p.type.fnParams[i], i);
				cppSig += pd;
				lambdaParams += pd;
				callArgs += cppCbArgToC(p.type.fnParams[i], i);
			}
			cSig += ")";
			cppSig += ")";
			src << "    std::function<" << cppSig << "> _cb_" << p.name
				<< ";\n";
			src << "    if (" << p.name << ") {\n";
			src << "        using _FnT_" << p.name << " = " << cSig << ";\n";
			src << "        auto _fn_" << p.name << " = reinterpret_cast<_FnT_"
				<< p.name << ">(" << p.name << ");\n";
			src << "        auto _iso_" << p.name
				<< " = bird_current_dart_isolate();\n";
			src << "        _cb_" << p.name << " = [_fn_" << p.name << ", _iso_"
				<< p.name << "](" << lambdaParams << ") {\n";
			src << "            BirdScopedDartIsolate _guard(_iso_" << p.name
				<< ");\n";
			src << "            _fn_" << p.name << "(" << callArgs << ");\n";
			src << "        };\n";
			src << "    }\n";
		}
	}
}

static void emitReturnStmt(std::ostream& src, const TypeInfo& retType,
						   const std::string& callExpr) {
	switch (retType.cat) {
		case TypeCat::Void:
			src << "    " << callExpr << ";\n";
			break;
		case TypeCat::StdString:
			src << "    return bird_string_dup(" << callExpr << ");\n";
			break;
		case TypeCat::Enum:
			src << "    return static_cast<int32_t>(" << callExpr << ");\n";
			break;
		case TypeCat::GlmVec2:
		case TypeCat::GlmVec3:
		case TypeCat::GlmVec4:
		case TypeCat::GlmQuat:
			src << "    return from_glm(" << callExpr << ");\n";
			break;
		case TypeCat::GlmMat3:
		case TypeCat::GlmMat4:
			src << "    *out_result = from_glm(" << callExpr << ");\n";
			break;
		case TypeCat::OpaqueClass:
			if (retType.isPointer) {
				src << "    return static_cast<void*>(" << callExpr << ");\n";
			} else if (retType.isReference) {
				src << "    return const_cast<void*>(static_cast<const void*>(&("
					<< callExpr << ")));\n";
			} else {
				src << "    return new ::" << retType.cppCanonical << "("
					<< callExpr << ");\n";
			}
			break;
		case TypeCat::SharedPtr:
			src << "    auto _sp = " << callExpr << ";\n";
			src << "    if (!_sp) return nullptr;\n";
			src << "    return new std::shared_ptr<::" << retType.cppCanonical
				<< ">(std::move(_sp));\n";
			break;
		case TypeCat::VectorInt:
			src << "    const auto& _v = " << callExpr << ";\n";
			src << "    BInt32List _out{nullptr, static_cast<int32_t>(_v.size())};\n";
			src << "    if (_out.length > 0) {\n";
			src << "        _out.data = static_cast<int32_t*>(std::malloc(sizeof(int32_t) * _out.length));\n";
			src << "        for (int32_t _i = 0; _i < _out.length; ++_i) _out.data[_i] = _v[_i];\n";
			src << "    }\n";
			src << "    return _out;\n";
			break;
		case TypeCat::VectorString:
			src << "    const auto& _v = " << callExpr << ";\n";
			src << "    BStrList _out{nullptr, static_cast<int32_t>(_v.size())};\n";
			src << "    if (_out.length > 0) {\n";
			src << "        _out.data = static_cast<char**>(std::malloc(sizeof(char*) * _out.length));\n";
			src << "        for (int32_t _i = 0; _i < _out.length; ++_i) _out.data[_i] = bird_string_dup(_v[_i]);\n";
			src << "    }\n";
			src << "    return _out;\n";
			break;
		case TypeCat::VectorOpaque:
			src << "    const auto& _v = " << callExpr << ";\n";
			src << "    BPtrList _out{nullptr, static_cast<int32_t>(_v.size())};\n";
			src << "    if (_out.length > 0) {\n";
			src << "        _out.data = static_cast<void**>(std::malloc(sizeof(void*) * _out.length));\n";
			if (retType.innerIsUniquePtr) {
				src << "        for (int32_t _i = 0; _i < _out.length; ++_i) _out.data[_i] = static_cast<void*>(_v[_i].get());\n";
			} else if (retType.innerIsValue) {
				src << "        for (int32_t _i = 0; _i < _out.length; ++_i) _out.data[_i] = new ::"
					<< retType.cppCanonical << "(_v[_i]);\n";
			} else {
				src << "        for (int32_t _i = 0; _i < _out.length; ++_i) _out.data[_i] = static_cast<void*>(_v[_i]);\n";
			}
			src << "    }\n";
			src << "    return _out;\n";
			break;
		case TypeCat::VectorShared:
			src << "    auto _v = " << callExpr << ";\n";
			src << "    BPtrList _out{nullptr, static_cast<int32_t>(_v.size())};\n";
			src << "    if (_out.length > 0) {\n";
			src << "        _out.data = static_cast<void**>(std::malloc(sizeof(void*) * _out.length));\n";
			src << "        for (int32_t _i = 0; _i < _out.length; ++_i) _out.data[_i] = new std::shared_ptr<::"
				<< retType.cppCanonical << ">(_v[_i]);\n";
			src << "    }\n";
			src << "    return _out;\n";
			break;
		default:
			src << "    return " << callExpr << ";\n";
			break;
	}
}

static std::string buildCParamList(const EmittedType& ret, bool hasSelf,
								   const TypeInfo& retInfo,
								   const std::vector<ParamInfo>& params,
								   const Model& model) {
	std::string out;
	bool first = true;
	auto add = [&](const std::string& type, const std::string& name) {
		if (!first) out += ", ";
		out += type + " " + name;
		first = false;
	};
	if (ret.isOutPtr) {
		add(retInfo.cat == TypeCat::GlmMat3 ? "BMat3*" : "BMat4*",
			"out_result");
	}
	if (hasSelf) add("void*", "self");
	for (const auto& p : params) {
		EmittedType pe = emitType(p.type, model, false);
		add(pe.cType, p.name);
		if (p.type.cat == TypeCat::VectorShared ||
			p.type.cat == TypeCat::VectorString ||
			(p.type.cat == TypeCat::VectorOpaque && p.type.innerIsValue)) {
			add("int32_t", p.name + "_count");
		}
	}
	if (out.empty()) out = "void";
	return out;
}

static void writeFiles(const fs::path& outDir,
					   const std::vector<fs::path>& sourceHeaders,
					   const Model& model) {
	fs::create_directories(outDir);
	std::ofstream hdr(outDir / "c_wrapper.h");
	std::ofstream src(outDir / "c_wrapper.cpp");

	// ----- Header prologue -----
	hdr << "// AUTO-GENERATED by DartApiGenerator. Do not edit by hand.\n";
	hdr << "#pragma once\n";
	hdr << "#include <stdint.h>\n";
	hdr << "#include <stdbool.h>\n\n";
	hdr << "#if defined(_WIN32) || defined(_MSC_VER)\n";
	hdr << "#define BIRD_C_API __declspec(dllexport)\n";
	hdr << "#else\n";
	hdr << "#define BIRD_C_API __attribute__((visibility(\"default\")))\n";
	hdr << "#endif\n\n";
	hdr << "#ifdef __cplusplus\nextern \"C\" {\n#endif\n\n";

	hdr << "// ---- GLM POD mirrors ----\n";
	hdr << "typedef struct BVec2 { float x, y; } BVec2;\n";
	hdr << "typedef struct BVec3 { float x, y, z; } BVec3;\n";
	hdr << "typedef struct BVec4 { float x, y, z, w; } BVec4;\n";
	hdr << "typedef struct BQuat { float x, y, z, w; } BQuat;\n";
	hdr << "typedef struct BMat3 { float m[9]; } BMat3;\n";
	hdr << "typedef struct BMat4 { float m[16]; } BMat4;\n\n";

	hdr << "// ---- List & String helpers ----\n";
	hdr << "typedef struct BInt32List { int32_t* data; int32_t length; } BInt32List;\n";
	hdr << "typedef struct BPtrList { void** data; int32_t length; } BPtrList;\n";
	hdr << "typedef struct BStrList { char** data; int32_t length; } BStrList;\n";
	hdr << "BIRD_C_API void bird_string_free(char* s);\n";
	hdr << "BIRD_C_API void bird_int32_list_free(BInt32List list);\n";
	hdr << "BIRD_C_API void bird_ptr_list_free(BPtrList list);\n";
	hdr << "BIRD_C_API void bird_str_list_free(BStrList list);\n\n";

	// ----- Impl prologue -----
	src << "// AUTO-GENERATED by DartApiGenerator. Do not edit by hand.\n";
	src << "#include \"c_wrapper.h\"\n";
	src << "#include <cstdlib>\n";
	src << "#include <cstring>\n";
	src << "#include <functional>\n";
	src << "#include <memory>\n";
	src << "#include <string>\n";
	src << "#include <utility>\n";
	src << "#include <vector>\n";
	src << "#include <glm/glm.hpp>\n";
	src << "#include <glm/gtc/quaternion.hpp>\n";
	src << "#include <glm/gtc/type_ptr.hpp>\n";
	src << "#ifdef BIRD_ENABLE_DART\n";
	src << "#include <dart_api.h>\n";
	src << "#endif\n";
	for (const auto& h : sourceHeaders) {
		src << "#include \"" << h.string() << "\"\n";
	}
	src << "\n";
	src << "#ifdef BIRD_ENABLE_DART\n";
	src << "static inline Dart_Isolate bird_current_dart_isolate() { return Dart_CurrentIsolate(); }\n";
	src << "struct BirdScopedDartIsolate {\n";
	src << "    bool entered = false;\n";
	src << "    explicit BirdScopedDartIsolate(Dart_Isolate iso) {\n";
	src << "        if (iso && Dart_CurrentIsolate() == nullptr) { Dart_EnterIsolate(iso); entered = true; }\n";
	src << "    }\n";
	src << "    ~BirdScopedDartIsolate() { if (entered) Dart_ExitIsolate(); }\n";
	src << "};\n";
	src << "#else\n";
	src << "static inline void* bird_current_dart_isolate() { return nullptr; }\n";
	src << "struct BirdScopedDartIsolate { explicit BirdScopedDartIsolate(void*) {} };\n";
	src << "#endif\n\n";
	src << "static inline glm::vec2 to_glm(BVec2 v) { return {v.x, v.y}; }\n";
	src << "static inline glm::vec3 to_glm(BVec3 v) { return {v.x, v.y, v.z}; }\n";
	src << "static inline glm::vec4 to_glm(BVec4 v) { return {v.x, v.y, v.z, v.w}; }\n";
	src << "static inline glm::quat to_glm(BQuat q) { return glm::quat(q.w, q.x, q.y, q.z); }\n";
	src << "static inline glm::mat3 to_glm(const BMat3& m) {\n";
	src << "    glm::mat3 r; for (int i = 0; i < 9; ++i) glm::value_ptr(r)[i] = m.m[i]; return r;\n";
	src << "}\n";
	src << "static inline glm::mat4 to_glm(const BMat4& m) {\n";
	src << "    glm::mat4 r; for (int i = 0; i < 16; ++i) glm::value_ptr(r)[i] = m.m[i]; return r;\n";
	src << "}\n";
	src << "static inline BVec2 from_glm(const glm::vec2& v) { return {v.x, v.y}; }\n";
	src << "static inline BVec3 from_glm(const glm::vec3& v) { return {v.x, v.y, v.z}; }\n";
	src << "static inline BVec4 from_glm(const glm::vec4& v) { return {v.x, v.y, v.z, v.w}; }\n";
	src << "static inline BQuat from_glm(const glm::quat& q) { return {q.x, q.y, q.z, q.w}; }\n";
	src << "static inline BMat3 from_glm(const glm::mat3& m) {\n";
	src << "    BMat3 r; for (int i = 0; i < 9; ++i) r.m[i] = glm::value_ptr(m)[i]; return r;\n";
	src << "}\n";
	src << "static inline BMat4 from_glm(const glm::mat4& m) {\n";
	src << "    BMat4 r; for (int i = 0; i < 16; ++i) r.m[i] = glm::value_ptr(m)[i]; return r;\n";
	src << "}\n";
	src << "static inline char* bird_string_dup(const std::string& s) {\n";
	src << "    char* out = static_cast<char*>(std::malloc(s.size() + 1));\n";
	src << "    std::memcpy(out, s.c_str(), s.size() + 1);\n";
	src << "    return out;\n";
	src << "}\n\n";

	src << "extern \"C\" {\n\n";
	src << "BIRD_C_API void bird_string_free(char* s) { std::free(s); }\n";
	src << "BIRD_C_API void bird_int32_list_free(BInt32List list) { std::free(list.data); }\n";
	src << "BIRD_C_API void bird_ptr_list_free(BPtrList list) { std::free(list.data); }\n";
	src << "BIRD_C_API void bird_str_list_free(BStrList list) {\n";
	src << "    for (int32_t i = 0; i < list.length; ++i) std::free(list.data[i]);\n";
	src << "    std::free(list.data);\n";
	src << "}\n\n";

	// ----- SharedPtr ref wrappers -----
	if (!model.sharedRefTypes.empty()) {
		hdr << "// ---- Shared Pointer Handle Deleters ----\n";
		src << "// ---- Shared Pointer Handle Deleters ----\n";
		for (const auto& [_, sr] : model.sharedRefTypes) {
			hdr << "BIRD_C_API void " << sr.flatName
				<< "_ref_delete(void* ref_ptr);\n";
			src << "BIRD_C_API void " << sr.flatName
				<< "_ref_delete(void* ref_ptr) {\n"
				<< "    delete static_cast<std::shared_ptr<::"
				<< sr.qualifiedName << ">*>(ref_ptr);\n}\n";
		}
		hdr << "\n";
		src << "\n";
	}

	// ----- Classes -----
	for (const auto& [_, cls] : model.classes) {
		hdr << "// ===== " << cls.qualifiedName << " =====\n";
		emitCDoc(hdr, cls.doc);
		src << "// ===== " << cls.qualifiedName << " =====\n";

		if (cls.hasPublicDestructor) {
			hdr << "BIRD_C_API void " << cls.flatName
				<< "_delete(void* self);\n";
			src << "BIRD_C_API void " << cls.flatName
				<< "_delete(void* self) {\n"
				<< "    delete static_cast<::" << cls.qualifiedName
				<< "*>(self);\n}\n\n";
		}

		// Public fields -> getter + setter
		for (const auto& f : cls.fields) {
			EmittedType getRet = emitType(f.type, model, true);
			EmittedType setParam = emitType(f.type, model, false);
			std::string getFn = cls.flatName + "_get_" + f.name;
			std::string setFn = cls.flatName + "_set_" + f.name;

			std::string getParams = buildCParamList(
				getRet, !f.isStatic, f.type, {}, model);
			emitCDoc(hdr, f.doc);
			hdr << "BIRD_C_API " << getRet.cType << " " << getFn << "("
				<< getParams << ");\n";
			src << "BIRD_C_API " << getRet.cType << " " << getFn << "("
				<< getParams << ") {\n";
			std::string fieldAccess =
				f.isStatic
					? ("::" + cls.qualifiedName + "::" + f.name)
					: ("static_cast<::" + cls.qualifiedName + "*>(self)->" +
					   f.name);
			emitReturnStmt(src, f.type, fieldAccess);
			src << "}\n";

			if (!f.isReadOnly) {
				ParamInfo vp{f.name, f.type, ""};
				std::string setParams = buildCParamList(
					{"void", "ffi.Void", "void", "void", false}, !f.isStatic,
					TypeInfo{}, {vp}, model);
				hdr << "BIRD_C_API void " << setFn << "(" << setParams
					<< ");\n";
				src << "BIRD_C_API void " << setFn << "(" << setParams
					<< ") {\n";
				emitVectorParamPrologue(src, {vp}, model);
				src << "    " << fieldAccess << " = " << cppArgExpr(vp)
					<< ";\n";
				src << "}\n\n";
			} else {
				src << "\n";
			}
		}

		// Methods & constructors
		for (const auto& m : cls.methods) {
			if (m.isDestructor) continue;
			EmittedType ret =
				m.isConstructor
					? EmittedType{"void*", "ffi.Pointer<ffi.Void>",
								  "ffi.Pointer<ffi.Void>", cls.dartName, false}
					: emitType(m.retType, model, true);

			std::string fnName = cls.flatName + "_" +
								 (m.isConstructor ? "new" : m.name) +
								 m.mangledSuffix;
			bool hasSelf = !m.isStatic && !m.isConstructor;
			std::string params =
				buildCParamList(ret, hasSelf, m.retType, m.params, model);

			emitCDoc(hdr, m.doc);
			hdr << "BIRD_C_API " << ret.cType << " " << fnName << "(" << params
				<< ");\n";

			src << "BIRD_C_API " << ret.cType << " " << fnName << "(" << params
				<< ") {\n";
			emitVectorParamPrologue(src, m.params, model);

			std::ostringstream args;
			for (size_t i = 0; i < m.params.size(); ++i) {
				if (i) args << ", ";
				args << cppArgExpr(m.params[i]);
			}

			if (m.isConstructor) {
				src << "    return new ::" << cls.qualifiedName << "("
					<< args.str() << ");\n}\n\n";
			} else {
				std::string call;
				if (hasSelf) {
					std::string cQual = m.isConst ? "const " : "";
					src << "    auto* _this = static_cast<" << cQual
						<< "::" << cls.qualifiedName << "*>(self);\n";
					call = "_this->" + m.name + "(" + args.str() + ")";
				} else {
					call = "::" + cls.qualifiedName + "::" + m.name + "(" +
						   args.str() + ")";
				}
				emitReturnStmt(src, m.retType, call);
				src << "}\n\n";
			}
		}
		hdr << "\n";
	}

	// ----- Free functions -----
	if (!model.freeFunctions.empty()) {
		hdr << "// ===== Free Functions =====\n";
		src << "// ===== Free Functions =====\n";
		for (const auto& fn : model.freeFunctions) {
			EmittedType ret = emitType(fn.retType, model, true);
			std::string cName =
				"bird_fn_" + fn.simpleName + fn.mangledSuffix;
			std::string params =
				buildCParamList(ret, false, fn.retType, fn.params, model);
			emitCDoc(hdr, fn.doc);
			hdr << "BIRD_C_API " << ret.cType << " " << cName << "(" << params
				<< ");\n";
			src << "BIRD_C_API " << ret.cType << " " << cName << "(" << params
				<< ") {\n";
			emitVectorParamPrologue(src, fn.params, model);
			std::ostringstream args;
			for (size_t i = 0; i < fn.params.size(); ++i) {
				if (i) args << ", ";
				args << cppArgExpr(fn.params[i]);
			}
			std::string call =
				"::" + fn.qualifiedName + "(" + args.str() + ")";
			emitReturnStmt(src, fn.retType, call);
			src << "}\n\n";
		}
		hdr << "\n";
	}

	// ----- Singletons -----
	if (!model.singletons.empty()) {
		hdr << "// ===== Global Singletons =====\n";
		src << "// ===== Global Singletons =====\n";
		for (const auto& [_, g] : model.singletons) {
			std::string cName = "bird_global_" + g.simpleName;
			emitCDoc(hdr, g.doc);
			hdr << "BIRD_C_API void* " << cName << "(void);\n";
			src << "BIRD_C_API void* " << cName << "(void) {\n"
				<< "    return static_cast<void*>(::" << g.qualifiedName
				<< ");\n}\n\n";
		}
	}

	hdr << "#ifdef __cplusplus\n}\n#endif\n";
	src << "} // extern \"C\"\n";
}

}  // namespace cwrap

// =============================================================================
// 8. Tier 2: Low-Level Dart FFI Bindings (bindings.dart)
// =============================================================================

namespace dffi {

static std::string ffiSig(const std::string& ret,
						  const std::vector<std::string>& args) {
	std::string s = ret + " Function(";
	for (size_t i = 0; i < args.size(); ++i) {
		if (i) s += ", ";
		s += args[i];
	}
	s += ")";
	return s;
}

static void emitLookup(std::ostream& f, const std::string& sym,
					   const std::string& nativeRet,
					   const std::string& dartRet,
					   const std::vector<std::string>& nativeArgs,
					   const std::vector<std::string>& dartArgs) {
	f << "late final " << dartRet << " Function(";
	for (size_t i = 0; i < dartArgs.size(); ++i) {
		if (i) f << ", ";
		f << dartArgs[i];
	}
	f << ") " << sym << " =\n"
	  << "    _dylib.lookupFunction<" << ffiSig(nativeRet, nativeArgs) << ", "
	  << ffiSig(dartRet, dartArgs) << ">('" << sym << "');\n";
}

static void writeFile(const fs::path& outDir, const std::string& libName,
					  const Model& model) {
	std::ofstream f(outDir / "bindings.dart");
	f << "// AUTO-GENERATED by DartApiGenerator. Do not edit by hand.\n";
	f << "// Low-level dart:ffi bindings.\n\n";
	f << "import 'dart:ffi' as ffi;\n";
	f << "import 'dart:io' show Platform;\n";
	f << "import 'package:ffi/ffi.dart' as pkg_ffi;\n\n";

	f << "ffi.DynamicLibrary? _customLib;\n\n";
	f << "/// Overrides the native library handle (useful for custom hosts/tests).\n";
	f << "void setNativeLibrary(ffi.DynamicLibrary lib) {\n";
	f << "  _customLib = lib;\n";
	f << "}\n\n";

	f << "late final ffi.DynamicLibrary _dylib = _customLib ?? _open();\n\n";
	f << "ffi.DynamicLibrary _open() {\n";
	f << "  // Prefer in-process symbols when running inside the embedded Bird executable.\n";
	f << "  final proc = ffi.DynamicLibrary.process();\n";
	f << "  if (proc.providesSymbol('bird_string_free')) {\n";
	f << "    return proc;\n";
	f << "  }\n";
	f << "  if (Platform.isWindows) return ffi.DynamicLibrary.open('" << libName
	  << ".dll');\n";
	f << "  if (Platform.isMacOS) return ffi.DynamicLibrary.open('" << libName
	  << ".dylib');\n";
	f << "  return ffi.DynamicLibrary.open('" << libName << ".so');\n";
	f << "}\n\n";
	f << "ffi.DynamicLibrary get nativeLib => _dylib;\n\n";

	// POD structs
	f << "// ---- GLM POD mirrors ----\n";
	f << "final class BVec2 extends ffi.Struct {\n"
	  << "  @ffi.Float() external double x;\n"
	  << "  @ffi.Float() external double y;\n}\n\n";
	f << "final class BVec3 extends ffi.Struct {\n"
	  << "  @ffi.Float() external double x;\n"
	  << "  @ffi.Float() external double y;\n"
	  << "  @ffi.Float() external double z;\n}\n\n";
	f << "final class BVec4 extends ffi.Struct {\n"
	  << "  @ffi.Float() external double x;\n"
	  << "  @ffi.Float() external double y;\n"
	  << "  @ffi.Float() external double z;\n"
	  << "  @ffi.Float() external double w;\n}\n\n";
	f << "final class BQuat extends ffi.Struct {\n"
	  << "  @ffi.Float() external double x;\n"
	  << "  @ffi.Float() external double y;\n"
	  << "  @ffi.Float() external double z;\n"
	  << "  @ffi.Float() external double w;\n}\n\n";
	f << "final class BMat3 extends ffi.Struct {\n"
	  << "  @ffi.Array.multi([9]) external ffi.Array<ffi.Float> m;\n}\n\n";
	f << "final class BMat4 extends ffi.Struct {\n"
	  << "  @ffi.Array.multi([16]) external ffi.Array<ffi.Float> m;\n}\n\n";

	f << "// ---- List & String POD helpers ----\n";
	f << "final class BInt32List extends ffi.Struct {\n"
	  << "  external ffi.Pointer<ffi.Int32> data;\n"
	  << "  @ffi.Int32() external int length;\n}\n\n";
	f << "final class BPtrList extends ffi.Struct {\n"
	  << "  external ffi.Pointer<ffi.Pointer<ffi.Void>> data;\n"
	  << "  @ffi.Int32() external int length;\n}\n\n";
	f << "final class BStrList extends ffi.Struct {\n"
	  << "  external ffi.Pointer<ffi.Pointer<pkg_ffi.Utf8>> data;\n"
	  << "  @ffi.Int32() external int length;\n}\n\n";

	emitLookup(f, "bird_string_free", "ffi.Void", "void",
			   {"ffi.Pointer<pkg_ffi.Utf8>"}, {"ffi.Pointer<pkg_ffi.Utf8>"});
	emitLookup(f, "bird_int32_list_free", "ffi.Void", "void", {"BInt32List"},
			   {"BInt32List"});
	emitLookup(f, "bird_ptr_list_free", "ffi.Void", "void", {"BPtrList"},
			   {"BPtrList"});
	emitLookup(f, "bird_str_list_free", "ffi.Void", "void", {"BStrList"},
			   {"BStrList"});
	f << "\n";

	// SharedPtr deleters
	if (!model.sharedRefTypes.empty()) {
		f << "// ---- SharedPtr Handle Deleters ----\n";
		for (const auto& [_, sr] : model.sharedRefTypes) {
			std::string sym = sr.flatName + "_ref_delete";
			f << "late final ffi.Pointer<ffi.NativeFunction<ffi.Void Function(ffi.Pointer<ffi.Void>)>> "
			  << sym << "Ptr =\n"
			  << "    _dylib.lookup<ffi.NativeFunction<ffi.Void Function(ffi.Pointer<ffi.Void>)>>('"
			  << sym << "');\n";
			f << "late final void Function(ffi.Pointer<ffi.Void>) " << sym
			  << " = " << sym << "Ptr.asFunction();\n";
		}
		f << "\n";
	}

	// Classes
	for (const auto& [_, cls] : model.classes) {
		f << "// ===== " << cls.qualifiedName << " =====\n";
		if (cls.hasPublicDestructor) {
			std::string dtorSym = cls.flatName + "_delete";
			f << "late final ffi.Pointer<ffi.NativeFunction<ffi.Void Function(ffi.Pointer<ffi.Void>)>> "
			  << dtorSym << "Ptr =\n"
			  << "    _dylib.lookup<ffi.NativeFunction<ffi.Void Function(ffi.Pointer<ffi.Void>)>>('"
			  << dtorSym << "');\n";
			f << "late final void Function(ffi.Pointer<ffi.Void>) " << dtorSym
			  << " = " << dtorSym << "Ptr.asFunction();\n";
		}

		// Fields
		for (const auto& field : cls.fields) {
			EmittedType getRet = emitType(field.type, model, true);
			EmittedType setArg = emitType(field.type, model, false);
			std::vector<std::string> gNat, gDart;
			if (getRet.isOutPtr) {
				std::string pt = field.type.cat == TypeCat::GlmMat3
									 ? "ffi.Pointer<BMat3>"
									 : "ffi.Pointer<BMat4>";
				gNat.push_back(pt);
				gDart.push_back(pt);
			}
			if (!field.isStatic) {
				gNat.push_back("ffi.Pointer<ffi.Void>");
				gDart.push_back("ffi.Pointer<ffi.Void>");
			}
			emitLookup(f, cls.flatName + "_get_" + field.name, getRet.ffiNative,
					   getRet.ffiDart, gNat, gDart);

			if (!field.isReadOnly) {
				std::vector<std::string> sNat, sDart;
				if (!field.isStatic) {
					sNat.push_back("ffi.Pointer<ffi.Void>");
					sDart.push_back("ffi.Pointer<ffi.Void>");
				}
				sNat.push_back(setArg.ffiNative);
				sDart.push_back(setArg.ffiDart);
				if (field.type.cat == TypeCat::VectorShared ||
					field.type.cat == TypeCat::VectorString ||
					(field.type.cat == TypeCat::VectorOpaque &&
					 field.type.innerIsValue)) {
					sNat.push_back("ffi.Int32");
					sDart.push_back("int");
				}
				emitLookup(f, cls.flatName + "_set_" + field.name, "ffi.Void",
						   "void", sNat, sDart);
			}
		}

		// Methods
		for (const auto& m : cls.methods) {
			if (m.isDestructor) continue;
			EmittedType ret =
				m.isConstructor
					? EmittedType{"void*", "ffi.Pointer<ffi.Void>",
								  "ffi.Pointer<ffi.Void>", cls.dartName, false}
					: emitType(m.retType, model, true);

			std::string sym = cls.flatName + "_" +
							  (m.isConstructor ? "new" : m.name) +
							  m.mangledSuffix;
			std::vector<std::string> natArgs, dartArgs;
			if (ret.isOutPtr) {
				std::string pt = m.retType.cat == TypeCat::GlmMat3
									 ? "ffi.Pointer<BMat3>"
									 : "ffi.Pointer<BMat4>";
				natArgs.push_back(pt);
				dartArgs.push_back(pt);
			}
			if (!m.isStatic && !m.isConstructor) {
				natArgs.push_back("ffi.Pointer<ffi.Void>");
				dartArgs.push_back("ffi.Pointer<ffi.Void>");
			}
			for (const auto& p : m.params) {
				EmittedType pe = emitType(p.type, model, false);
				natArgs.push_back(pe.ffiNative);
				dartArgs.push_back(pe.ffiDart);
				if (p.type.cat == TypeCat::VectorShared ||
					p.type.cat == TypeCat::VectorString ||
					(p.type.cat == TypeCat::VectorOpaque &&
					 p.type.innerIsValue)) {
					natArgs.push_back("ffi.Int32");
					dartArgs.push_back("int");
				}
			}
			emitLookup(f, sym, ret.ffiNative, ret.ffiDart, natArgs, dartArgs);
		}
		f << "\n";
	}

	// Free functions
	if (!model.freeFunctions.empty()) {
		f << "// ===== Free Functions =====\n";
		for (const auto& fn : model.freeFunctions) {
			EmittedType ret = emitType(fn.retType, model, true);
			std::string sym = "bird_fn_" + fn.simpleName + fn.mangledSuffix;
			std::vector<std::string> natArgs, dartArgs;
			if (ret.isOutPtr) {
				std::string pt = fn.retType.cat == TypeCat::GlmMat3
									 ? "ffi.Pointer<BMat3>"
									 : "ffi.Pointer<BMat4>";
				natArgs.push_back(pt);
				dartArgs.push_back(pt);
			}
			for (const auto& p : fn.params) {
				EmittedType pe = emitType(p.type, model, false);
				natArgs.push_back(pe.ffiNative);
				dartArgs.push_back(pe.ffiDart);
				if (p.type.cat == TypeCat::VectorShared ||
					p.type.cat == TypeCat::VectorString ||
					(p.type.cat == TypeCat::VectorOpaque &&
					 p.type.innerIsValue)) {
					natArgs.push_back("ffi.Int32");
					dartArgs.push_back("int");
				}
			}
			emitLookup(f, sym, ret.ffiNative, ret.ffiDart, natArgs, dartArgs);
		}
		f << "\n";
	}

	// Singletons
	if (!model.singletons.empty()) {
		f << "// ===== Global Singletons =====\n";
		for (const auto& [_, g] : model.singletons) {
			emitLookup(f, "bird_global_" + g.simpleName,
					   "ffi.Pointer<ffi.Void>", "ffi.Pointer<ffi.Void>", {},
					   {});
		}
	}
}

}  // namespace dffi

// =============================================================================
// 9. Tier 3: High-Level Dart Proxy Generation
// =============================================================================

namespace dproxy {

static void writeNativeObjectBase(const fs::path& outDir) {
	std::ofstream f(outDir / "native_object.dart");
	f << "// AUTO-GENERATED by DartApiGenerator. Do not edit by hand.\n\n";
	f << "import 'dart:ffi' as ffi;\n\n";
	f << "/// Base class for all Bird Engine native C++ proxy objects.\n";
	f << "///\n";
	f << "/// Tracks ownership semantics so Dart GC only destroys C++ instances\n";
	f << "/// that Dart owns, never engine-owned borrowed references.\n";
	f << "abstract class BirdNativeObject implements ffi.Finalizable {\n";
	f << "  /// Raw native pointer to the underlying C++ object.\n";
	f << "  final ffi.Pointer<ffi.Void> handle;\n";
	f << "  final ffi.NativeFinalizer? _finalizer;\n";
	f << "  final void Function(ffi.Pointer<ffi.Void>)? _deleter;\n";
	f << "  bool _owned;\n";
	f << "  bool _disposed = false;\n\n";
	f << "  BirdNativeObject.fromHandle(\n";
	f << "    this.handle, {\n";
	f << "    bool owned = false,\n";
	f << "    ffi.NativeFinalizer? finalizer,\n";
	f << "    void Function(ffi.Pointer<ffi.Void>)? deleter,\n";
	f << "  })  : _owned = owned,\n";
	f << "        _finalizer = finalizer,\n";
	f << "        _deleter = deleter {\n";
	f << "    if (_owned && _finalizer != null && handle != ffi.nullptr) {\n";
	f << "      _finalizer.attach(this, handle.cast(), detach: this);\n";
	f << "    }\n";
	f << "  }\n\n";
	f << "  /// Whether Dart currently owns the underlying C++ instance.\n";
	f << "  bool get isOwned => _owned;\n\n";
	f << "  /// Whether [dispose] has already destroyed this instance.\n";
	f << "  bool get isDisposed => _disposed;\n\n";
	f << "  /// Detaches the native finalizer and transfers ownership to C++.\n";
	f << "  void releaseOwnership() {\n";
	f << "    if (_owned && _finalizer != null) {\n";
	f << "      _finalizer.detach(this);\n";
	f << "    }\n";
	f << "    _owned = false;\n";
	f << "  }\n\n";
	f << "  /// Deterministically destroys the underlying C++ object if owned by Dart.\n";
	f << "  void dispose() {\n";
	f << "    if (_disposed || !_owned) return;\n";
	f << "    if (_finalizer != null) {\n";
	f << "      _finalizer.detach(this);\n";
	f << "    }\n";
	f << "    if (_deleter != null && handle != ffi.nullptr) {\n";
	f << "      _deleter(handle);\n";
	f << "    }\n";
	f << "    _owned = false;\n";
	f << "    _disposed = true;\n";
	f << "  }\n";
	f << "}\n\n";
	f << "final List<ffi.NativeCallable<dynamic>> _registeredCallbacks = [];\n\n";
	f << "/// Retains a [ffi.NativeCallable] while C++ holds its native pointer.\n";
	f << "T registerNativeCallback<T extends ffi.NativeCallable<dynamic>>(T cb) {\n";
	f << "  _registeredCallbacks.add(cb);\n";
	f << "  return cb;\n";
	f << "}\n\n";
	f << "/// Closes all registered native callbacks (call on shutdown).\n";
	f << "void clearNativeCallbacks() {\n";
	f << "  for (final cb in _registeredCallbacks) {\n";
	f << "    cb.close();\n";
	f << "  }\n";
	f << "  _registeredCallbacks.clear();\n";
	f << "}\n";
}

static void writeMathFile(const fs::path& outDir) {
	std::ofstream f(outDir / "math.dart");
	f << R"DART(// AUTO-GENERATED by DartApiGenerator. Do not edit by hand.
// User-facing math types mirroring bird::Vector2/3/4, Quaternion, Matrix3/4.

import 'dart:ffi' as ffi;
import 'dart:math' as math;
import 'bindings.dart' as b;

/// 2D floating-point vector mirroring `bird::Vector2` (`glm::vec2`).
class Vector2 {
  double x;
  double y;

  Vector2(this.x, this.y);
  Vector2.zero()
      : x = 0.0,
        y = 0.0;

  Vector2 operator +(Vector2 o) => Vector2(x + o.x, y + o.y);
  Vector2 operator -(Vector2 o) => Vector2(x - o.x, y - o.y);
  Vector2 operator -() => Vector2(-x, -y);
  Vector2 operator *(double s) => Vector2(x * s, y * s);
  Vector2 operator /(double s) => Vector2(x / s, y / s);

  double dot(Vector2 o) => x * o.x + y * o.y;
  double get length => math.sqrt(x * x + y * y);
  Vector2 normalized() {
    final l = length;
    return l == 0.0 ? Vector2.zero() : this / l;
  }

  @override
  bool operator ==(Object other) =>
      other is Vector2 && x == other.x && y == other.y;

  @override
  int get hashCode => Object.hash(x, y);

  @override
  String toString() => 'Vector2($x, $y)';
}

/// 3D floating-point vector mirroring `bird::Vector3` (`glm::vec3`).
class Vector3 {
  double x;
  double y;
  double z;

  Vector3(this.x, this.y, this.z);
  Vector3.zero()
      : x = 0.0,
        y = 0.0,
        z = 0.0;

  Vector3 operator +(Vector3 o) => Vector3(x + o.x, y + o.y, z + o.z);
  Vector3 operator -(Vector3 o) => Vector3(x - o.x, y - o.y, z - o.z);
  Vector3 operator -() => Vector3(-x, -y, -z);
  Vector3 operator *(double s) => Vector3(x * s, y * s, z * s);
  Vector3 operator /(double s) => Vector3(x / s, y / s, z / s);

  double dot(Vector3 o) => x * o.x + y * o.y + z * o.z;
  Vector3 cross(Vector3 o) => Vector3(
        y * o.z - z * o.y,
        z * o.x - x * o.z,
        x * o.y - y * o.x,
      );

  double get length => math.sqrt(x * x + y * y + z * z);
  Vector3 normalized() {
    final l = length;
    return l == 0.0 ? Vector3.zero() : this / l;
  }

  @override
  bool operator ==(Object other) =>
      other is Vector3 && x == other.x && y == other.y && z == other.z;

  @override
  int get hashCode => Object.hash(x, y, z);

  @override
  String toString() => 'Vector3($x, $y, $z)';
}

/// 4D floating-point vector mirroring `bird::Vector4` (`glm::vec4`).
class Vector4 {
  double x;
  double y;
  double z;
  double w;

  Vector4(this.x, this.y, this.z, this.w);
  Vector4.zero()
      : x = 0.0,
        y = 0.0,
        z = 0.0,
        w = 0.0;

  Vector4 operator +(Vector4 o) => Vector4(x + o.x, y + o.y, z + o.z, w + o.w);
  Vector4 operator -(Vector4 o) => Vector4(x - o.x, y - o.y, z - o.z, w - o.w);
  Vector4 operator -() => Vector4(-x, -y, -z, -w);
  Vector4 operator *(double s) => Vector4(x * s, y * s, z * s, w * s);
  Vector4 operator /(double s) => Vector4(x / s, y / s, z / s, w / s);

  @override
  bool operator ==(Object other) =>
      other is Vector4 &&
      x == other.x &&
      y == other.y &&
      z == other.z &&
      w == other.w;

  @override
  int get hashCode => Object.hash(x, y, z, w);

  @override
  String toString() => 'Vector4($x, $y, $z, $w)';
}

/// Unit quaternion mirroring `bird::Quaternion` (`glm::quat`).
class Quaternion {
  double x;
  double y;
  double z;
  double w;

  Quaternion(this.x, this.y, this.z, this.w);
  Quaternion.identity()
      : x = 0.0,
        y = 0.0,
        z = 0.0,
        w = 1.0;

  /// Composes two rotations (Hamilton product).
  Quaternion operator *(Quaternion q) => Quaternion(
        w * q.x + x * q.w + y * q.z - z * q.y,
        w * q.y - x * q.z + y * q.w + z * q.x,
        w * q.z + x * q.y - y * q.x + z * q.w,
        w * q.w - x * q.x - y * q.y - z * q.z,
      );

  /// Returns the conjugate (inverse for unit quaternions).
  Quaternion conjugate() => Quaternion(-x, -y, -z, w);

  /// Rotates a 3D vector by this quaternion (`q * v`).
  Vector3 rotate(Vector3 v) {
    final u = Vector3(x, y, z);
    final t = u.cross(v) * 2.0;
    return v + t * w + u.cross(t);
  }

  @override
  bool operator ==(Object other) =>
      other is Quaternion &&
      x == other.x &&
      y == other.y &&
      z == other.z &&
      w == other.w;

  @override
  int get hashCode => Object.hash(x, y, z, w);

  @override
  String toString() => 'Quaternion($x, $y, $z, $w)';
}

/// Column-major 3x3 matrix mirroring `bird::Matrix3` (`glm::mat3`).
class Matrix3 {
  final List<double> m;

  Matrix3(this.m) : assert(m.length == 9);
  Matrix3.identity() : m = List<double>.filled(9, 0.0) {
    m[0] = m[4] = m[8] = 1.0;
  }
}

/// Column-major 4x4 matrix mirroring `bird::Matrix4` (`glm::mat4`).
class Matrix4 {
  final List<double> m;

  Matrix4(this.m) : assert(m.length == 16);
  Matrix4.identity() : m = List<double>.filled(16, 0.0) {
    m[0] = m[5] = m[10] = m[15] = 1.0;
  }
}

// ---- Internal FFI marshalling helpers ----

ffi.Pointer<b.BVec2> packVec2(ffi.Allocator a, Vector2 v) {
  final p = a<b.BVec2>();
  p.ref
    ..x = v.x
    ..y = v.y;
  return p;
}

Vector2 unpackVec2(b.BVec2 s) => Vector2(s.x, s.y);

ffi.Pointer<b.BVec3> packVec3(ffi.Allocator a, Vector3 v) {
  final p = a<b.BVec3>();
  p.ref
    ..x = v.x
    ..y = v.y
    ..z = v.z;
  return p;
}

Vector3 unpackVec3(b.BVec3 s) => Vector3(s.x, s.y, s.z);

ffi.Pointer<b.BVec4> packVec4(ffi.Allocator a, Vector4 v) {
  final p = a<b.BVec4>();
  p.ref
    ..x = v.x
    ..y = v.y
    ..z = v.z
    ..w = v.w;
  return p;
}

Vector4 unpackVec4(b.BVec4 s) => Vector4(s.x, s.y, s.z, s.w);

ffi.Pointer<b.BQuat> packQuat(ffi.Allocator a, Quaternion v) {
  final p = a<b.BQuat>();
  p.ref
    ..x = v.x
    ..y = v.y
    ..z = v.z
    ..w = v.w;
  return p;
}

Quaternion unpackQuat(b.BQuat s) => Quaternion(s.x, s.y, s.z, s.w);

ffi.Pointer<b.BMat3> packMat3(ffi.Allocator a, Matrix3 v) {
  final p = a<b.BMat3>();
  for (var i = 0; i < 9; i++) {
    p.ref.m[i] = v.m[i];
  }
  return p;
}

Matrix3 unpackMat3(b.BMat3 s) {
  final l = List<double>.filled(9, 0.0);
  for (var i = 0; i < 9; i++) {
    l[i] = s.m[i];
  }
  return Matrix3(l);
}

ffi.Pointer<b.BMat4> packMat4(ffi.Allocator a, Matrix4 v) {
  final p = a<b.BMat4>();
  for (var i = 0; i < 16; i++) {
    p.ref.m[i] = v.m[i];
  }
  return p;
}

Matrix4 unpackMat4(b.BMat4 s) {
  final l = List<double>.filled(16, 0.0);
  for (var i = 0; i < 16; i++) {
    l[i] = s.m[i];
  }
  return Matrix4(l);
}
)DART";
}

static void writeEnumsFile(const fs::path& outDir, const Model& model) {
	std::ofstream f(outDir / "enums.dart");
	f << "// AUTO-GENERATED by DartApiGenerator. Do not edit by hand.\n\n";
	for (const auto& [_, en] : model.enums) {
		emitDartDoc(f, en.doc, "");
		f << "enum " << en.simpleName << " {\n";
		for (size_t i = 0; i < en.values.size(); ++i) {
			const auto& v = en.values[i];
			emitDartDoc(f, v.doc, "  ");
			f << "  " << safeDartIdent(v.name) << "(" << v.value << ")"
			  << (i + 1 < en.values.size() ? ",\n" : ";\n\n");
		}
		f << "  final int value;\n";
		f << "  const " << en.simpleName << "(this.value);\n\n";
		f << "  static " << en.simpleName << " fromValue(int value) {\n";
		f << "    for (final v in values) {\n";
		f << "      if (v.value == value) return v;\n";
		f << "    }\n";
		f << "    throw ArgumentError('Unknown " << en.simpleName
		  << " value: $value');\n";
		f << "  }\n";
		f << "}\n\n";
	}
}

static void writeSharedRefsFile(const fs::path& outDir, const Model& model) {
	std::ofstream f(outDir / "shared_refs.dart");
	f << "// AUTO-GENERATED by DartApiGenerator. Do not edit by hand.\n\n";
	f << "import 'dart:ffi' as ffi;\n";
	f << "import 'bindings.dart' as b;\n";
	f << "import 'native_object.dart';\n\n";

	for (const auto& [_, sr] : model.sharedRefTypes) {
		f << "/// Reference-counted shared handle mirroring `std::shared_ptr<"
		  << sr.qualifiedName << ">`.\n";
		f << "class " << sr.dartName << " extends BirdNativeObject {\n";
		f << "  static final ffi.NativeFinalizer _finalizer =\n"
		  << "      ffi.NativeFinalizer(b." << sr.flatName
		  << "_ref_deletePtr.cast());\n\n";
		f << "  " << sr.dartName
		  << ".fromHandle(super.handle, {super.owned = true})\n"
		  << "      : super.fromHandle(\n"
		  << "          finalizer: _finalizer,\n"
		  << "          deleter: b." << sr.flatName << "_ref_delete,\n"
		  << "        );\n";
		f << "}\n\n";
	}
}

static void writeInputConstantsFile(const fs::path& outDir, const Model& model) {
	std::ofstream f(outDir / "input_constants.dart");
	f << "// AUTO-GENERATED by DartApiGenerator. Do not edit by hand.\n";
	f << "// Keyboard, mouse, cursor, and action constants from Input.h.\n\n";
	for (const auto& c : model.macroConstants) {
		f << "const int " << c.name << " = " << c.value << ";\n";
	}
}

static std::optional<std::string> dartDefaultFor(const ParamInfo& p,
												 const Model& model) {
	if (p.defaultExpr.empty()) return std::nullopt;
	std::string e = trim(p.defaultExpr);
	if (p.type.cat == TypeCat::Bool && (e == "true" || e == "false")) return e;
	if (p.type.cat == TypeCat::Float || p.type.cat == TypeCat::Double ||
		p.type.cat == TypeCat::Int32 || p.type.cat == TypeCat::UInt32 ||
		p.type.cat == TypeCat::Int64 || p.type.cat == TypeCat::UInt64) {
		bool ok = !e.empty();
		for (char c : e) {
			if (!(std::isdigit(static_cast<unsigned char>(c)) || c == '.' ||
				  c == '-' || c == '+' || c == 'e' || c == 'E' || c == 'f' ||
				  c == 'F')) {
				ok = false;
				break;
			}
		}
		if (ok) {
			if (e.back() == 'f' || e.back() == 'F') e.pop_back();
			return e;
		}
	}
	if ((p.type.cat == TypeCat::StdString || p.type.cat == TypeCat::CString) &&
		e.size() >= 2 && e.front() == '"' && e.back() == '"') {
		return "'" + e.substr(1, e.size() - 2) + "'";
	}
	if (p.type.cat == TypeCat::Enum && model.enums.count(p.type.cppCanonical)) {
		// e.g. "UIAxis :: Vertical" -> "UIAxis.Vertical"
		std::string compact = replaceAll(e, " ", "");
		std::string enName = model.enums.at(p.type.cppCanonical).simpleName;
		std::string prefix = enName + "::";
		if (startsWith(compact, prefix)) {
			return enName + "." + safeDartIdent(compact.substr(prefix.size()));
		}
	}
	if ((p.type.cat == TypeCat::VectorString ||
		 p.type.cat == TypeCat::VectorOpaque ||
		 p.type.cat == TypeCat::VectorShared) &&
		(e == "{ }" || e == "{}")) {
		return std::string("const []");
	}
	if (p.type.cat == TypeCat::StdFunction && !e.empty()) {
		return std::string("null");
	}
	return std::nullopt;
}

static std::string buildDartParamList(const std::vector<ParamInfo>& params,
									  const Model& model) {
	// Only trailing parameters with valid Dart defaults can be enclosed in [...].
	size_t firstOpt = params.size();
	for (size_t i = params.size(); i > 0; --i) {
		if (dartDefaultFor(params[i - 1], model).has_value())
			firstOpt = i - 1;
		else
			break;
	}
	std::string out;
	for (size_t i = 0; i < params.size(); ++i) {
		if (i > 0) out += ", ";
		if (i == firstOpt) out += "[";
		EmittedType pe = emitType(params[i].type, model, false);
		std::string userT = pe.dartUser;
		if (i >= firstOpt && params[i].type.cat == TypeCat::StdFunction) {
			userT += "?";
		}
		out += userT + " " + safeDartIdent(params[i].name);
		if (i >= firstOpt) {
			std::string dv = *dartDefaultFor(params[i], model);
			if (dv != "null") out += " = " + dv;
		}
	}
	if (firstOpt < params.size()) out += "]";
	return out;
}

static std::string dartCbNativeType(const TypeInfo& t) {
	switch (t.cat) {
		case TypeCat::Bool:
			return "ffi.Bool";
		case TypeCat::Int32:
		case TypeCat::Enum:
			return "ffi.Int32";
		case TypeCat::UInt32:
			return "ffi.Uint32";
		case TypeCat::Int64:
			return "ffi.Int64";
		case TypeCat::UInt64:
			return "ffi.Uint64";
		case TypeCat::Float:
			return "ffi.Float";
		case TypeCat::Double:
			return "ffi.Double";
		case TypeCat::CString:
		case TypeCat::StdString:
			return "ffi.Pointer<Utf8>";
		case TypeCat::GlmVec2:
			return "b.BVec2";
		case TypeCat::GlmVec3:
			return "b.BVec3";
		case TypeCat::GlmVec4:
			return "b.BVec4";
		case TypeCat::GlmQuat:
			return "b.BQuat";
		default:
			return "ffi.Int32";
	}
}

static std::string dartCbDartParamType(const TypeInfo& t) {
	switch (t.cat) {
		case TypeCat::Bool:
			return "bool";
		case TypeCat::Int32:
		case TypeCat::UInt32:
		case TypeCat::Int64:
		case TypeCat::UInt64:
		case TypeCat::Enum:
			return "int";
		case TypeCat::Float:
		case TypeCat::Double:
			return "double";
		case TypeCat::CString:
		case TypeCat::StdString:
			return "ffi.Pointer<Utf8>";
		case TypeCat::GlmVec2:
			return "b.BVec2";
		case TypeCat::GlmVec3:
			return "b.BVec3";
		case TypeCat::GlmVec4:
			return "b.BVec4";
		case TypeCat::GlmQuat:
			return "b.BQuat";
		default:
			return "int";
	}
}

static std::string dartCbUnpackArg(const TypeInfo& t, const std::string& v,
								   const Model& model) {
	switch (t.cat) {
		case TypeCat::CString:
		case TypeCat::StdString:
			return v + " == ffi.nullptr ? '' : " + v + ".toDartString()";
		case TypeCat::GlmVec2:
			return "unpackVec2(" + v + ")";
		case TypeCat::GlmVec3:
			return "unpackVec3(" + v + ")";
		case TypeCat::GlmVec4:
			return "unpackVec4(" + v + ")";
		case TypeCat::GlmQuat:
			return "unpackQuat(" + v + ")";
		case TypeCat::Enum:
			return model.enums.at(t.cppCanonical).simpleName + ".fromValue(" +
				   v + ")";
		default:
			return v;
	}
}

static void emitDartCallArgs(std::ostream& f,
							 const std::vector<ParamInfo>& params,
							 std::vector<std::string>& callArgs,
							 const Model& model) {
	// Determine which trailing params are optional (for nullable StdFunction).
	size_t firstOpt = params.size();
	for (size_t i = params.size(); i > 0; --i) {
		if (dartDefaultFor(params[i - 1], model).has_value())
			firstOpt = i - 1;
		else
			break;
	}

	for (size_t idx = 0; idx < params.size(); ++idx) {
		const auto& p = params[idx];
		std::string n = safeDartIdent(p.name);
		bool isNullableFn =
			(p.type.cat == TypeCat::StdFunction && idx >= firstOpt);
		switch (p.type.cat) {
			case TypeCat::GlmVec2:
				callArgs.push_back("packVec2(arena, " + n + ").ref");
				break;
			case TypeCat::GlmVec3:
				callArgs.push_back("packVec3(arena, " + n + ").ref");
				break;
			case TypeCat::GlmVec4:
				callArgs.push_back("packVec4(arena, " + n + ").ref");
				break;
			case TypeCat::GlmQuat:
				callArgs.push_back("packQuat(arena, " + n + ").ref");
				break;
			case TypeCat::GlmMat3:
				callArgs.push_back("packMat3(arena, " + n + ")");
				break;
			case TypeCat::GlmMat4:
				callArgs.push_back("packMat4(arena, " + n + ")");
				break;
			case TypeCat::Enum:
				callArgs.push_back(n + ".value");
				break;
			case TypeCat::OpaqueClass:
			case TypeCat::SharedPtr:
				callArgs.push_back(n + ".handle");
				break;
			case TypeCat::UniquePtr:
				f << "      " << n << ".releaseOwnership();\n";
				callArgs.push_back(n + ".handle");
				break;
			case TypeCat::CString:
			case TypeCat::StdString:
				callArgs.push_back(n + ".toNativeUtf8(allocator: arena)");
				break;
			case TypeCat::VectorString:
				f << "      final _" << n
				  << "Ptr = arena<ffi.Pointer<Utf8>>(" << n << ".length);\n";
				f << "      for (var _i = 0; _i < " << n
				  << ".length; _i++) {\n";
				f << "        _" << n << "Ptr[_i] = " << n
				  << "[_i].toNativeUtf8(allocator: arena);\n";
				f << "      }\n";
				callArgs.push_back("_" + n + "Ptr");
				callArgs.push_back(n + ".length");
				break;
			case TypeCat::VectorShared:
			case TypeCat::VectorOpaque:
				f << "      final _" << n
				  << "Ptr = arena<ffi.Pointer<ffi.Void>>(" << n << ".length);\n";
				f << "      for (var _i = 0; _i < " << n
				  << ".length; _i++) {\n";
				f << "        _" << n << "Ptr[_i] = " << n << "[_i].handle;\n";
				f << "      }\n";
				callArgs.push_back("_" + n + "Ptr");
				callArgs.push_back(n + ".length");
				break;
			case TypeCat::StdFunction: {
				std::string natSig = "ffi.Void Function(";
				std::string cbParams;
				std::string cbArgs;
				for (size_t i = 0; i < p.type.fnParams.size(); ++i) {
					if (i) {
						natSig += ", ";
						cbParams += ", ";
						cbArgs += ", ";
					}
					natSig += dartCbNativeType(p.type.fnParams[i]);
					std::string an = "_a" + std::to_string(i);
					cbParams += dartCbDartParamType(p.type.fnParams[i]) + " " + an;
					cbArgs += dartCbUnpackArg(p.type.fnParams[i], an, model);
				}
				natSig += ")";
				std::string ptrVar = "_" + n + "Ptr";
				if (isNullableFn) {
					f << "      final ffi.Pointer<ffi.Void> " << ptrVar << " = "
					  << n << " == null ? ffi.nullptr : registerNativeCallback("
					  << "ffi.NativeCallable<" << natSig << ">.isolateLocal(("
					  << cbParams << ") { " << n << "(" << cbArgs
					  << "); })).nativeFunction.cast<ffi.Void>();\n";
				} else {
					f << "      final ffi.Pointer<ffi.Void> " << ptrVar
					  << " = registerNativeCallback(ffi.NativeCallable<"
					  << natSig << ">.isolateLocal((" << cbParams << ") { " << n
					  << "(" << cbArgs
					  << "); })).nativeFunction.cast<ffi.Void>();\n";
				}
				callArgs.push_back(ptrVar);
				break;
			}
			default:
				callArgs.push_back(n);
				break;
		}
	}
}

static void emitDartReturn(std::ostream& f, const TypeInfo& retType,
						   const std::string& callExpr, const Model& model) {
	switch (retType.cat) {
		case TypeCat::Void:
			f << "      " << callExpr << ";\n";
			break;
		case TypeCat::GlmVec2:
			f << "      return unpackVec2(" << callExpr << ");\n";
			break;
		case TypeCat::GlmVec3:
			f << "      return unpackVec3(" << callExpr << ");\n";
			break;
		case TypeCat::GlmVec4:
			f << "      return unpackVec4(" << callExpr << ");\n";
			break;
		case TypeCat::GlmQuat:
			f << "      return unpackQuat(" << callExpr << ");\n";
			break;
		case TypeCat::GlmMat3:
			f << "      " << callExpr << ";\n";
			f << "      return unpackMat3(out.ref);\n";
			break;
		case TypeCat::GlmMat4:
			f << "      " << callExpr << ";\n";
			f << "      return unpackMat4(out.ref);\n";
			break;
		case TypeCat::Enum: {
			std::string en = model.enums.at(retType.cppCanonical).simpleName;
			f << "      return " << en << ".fromValue(" << callExpr << ");\n";
			break;
		}
		case TypeCat::CString:
			f << "      final p = " << callExpr << ";\n";
			f << "      return p == ffi.nullptr ? '' : p.toDartString();\n";
			break;
		case TypeCat::StdString:
			f << "      final p = " << callExpr << ";\n";
			f << "      if (p == ffi.nullptr) return '';\n";
			f << "      try {\n";
			f << "        return p.toDartString();\n";
			f << "      } finally {\n";
			f << "        b.bird_string_free(p);\n";
			f << "      }\n";
			break;
		case TypeCat::OpaqueClass: {
			std::string dn = model.classes.at(retType.cppCanonical).dartName;
			bool owned = !retType.isPointer && !retType.isReference;
			f << "      final p = " << callExpr << ";\n";
			if (retType.isPointer) {
				f << "      if (p == ffi.nullptr) return null;\n";
			}
			f << "      return " << dn << ".fromHandle(p, owned: "
			  << (owned ? "true" : "false") << ");\n";
			break;
		}
		case TypeCat::SharedPtr: {
			std::string dn =
				model.sharedRefTypes.at(retType.cppCanonical).dartName;
			f << "      final p = " << callExpr << ";\n";
			f << "      if (p == ffi.nullptr) return null;\n";
			f << "      return " << dn << ".fromHandle(p, owned: true);\n";
			break;
		}
		case TypeCat::VectorInt:
			f << "      final raw = " << callExpr << ";\n";
			f << "      try {\n";
			f << "        return [for (var i = 0; i < raw.length; i++) raw.data[i]];\n";
			f << "      } finally {\n";
			f << "        b.bird_int32_list_free(raw);\n";
			f << "      }\n";
			break;
		case TypeCat::VectorString:
			f << "      final raw = " << callExpr << ";\n";
			f << "      try {\n";
			f << "        return [\n";
			f << "          for (var i = 0; i < raw.length; i++)\n";
			f << "            raw.data[i] == ffi.nullptr ? '' : raw.data[i].toDartString(),\n";
			f << "        ];\n";
			f << "      } finally {\n";
			f << "        b.bird_str_list_free(raw);\n";
			f << "      }\n";
			break;
		case TypeCat::VectorOpaque: {
			std::string dn = model.classes.at(retType.cppCanonical).dartName;
			f << "      final raw = " << callExpr << ";\n";
			f << "      try {\n";
			f << "        return [\n";
			f << "          for (var i = 0; i < raw.length; i++)\n";
			f << "            " << dn << ".fromHandle(raw.data[i], owned: "
			  << (retType.innerIsValue ? "true" : "false") << "),\n";
			f << "        ];\n";
			f << "      } finally {\n";
			f << "        b.bird_ptr_list_free(raw);\n";
			f << "      }\n";
			break;
		}
		case TypeCat::VectorShared: {
			std::string dn =
				model.sharedRefTypes.at(retType.cppCanonical).dartName;
			f << "      final raw = " << callExpr << ";\n";
			f << "      try {\n";
			f << "        return [\n";
			f << "          for (var i = 0; i < raw.length; i++)\n";
			f << "            " << dn
			  << ".fromHandle(raw.data[i], owned: true),\n";
			f << "        ];\n";
			f << "      } finally {\n";
			f << "        b.bird_ptr_list_free(raw);\n";
			f << "      }\n";
			break;
		}
		default:
			f << "      return " << callExpr << ";\n";
			break;
	}
}

static void writeCommonImports(std::ostream& f, const Model& model,
							   const std::string& excludeClass = "") {
	f << "// ignore_for_file: unused_import\n";
	f << "import 'dart:ffi' as ffi;\n";
	f << "import 'package:ffi/ffi.dart';\n";
	f << "import 'bindings.dart' as b;\n";
	f << "import 'enums.dart';\n";
	f << "import 'math.dart';\n";
	f << "import 'native_object.dart';\n";
	f << "import 'shared_refs.dart';\n";
	for (const auto& [qn, other] : model.classes) {
		if (qn == excludeClass) continue;
		f << "import '" << toSnakeCase(other.dartName) << ".dart';\n";
	}
	f << "\n";
}

static void writeClassFile(const fs::path& outDir, const ClassInfo& cls,
						   const Model& model) {
	std::ofstream f(outDir / (toSnakeCase(cls.dartName) + ".dart"));
	f << "// AUTO-GENERATED by DartApiGenerator. Do not edit by hand.\n";
	f << "// Proxy for " << cls.qualifiedName << ".\n\n";
	writeCommonImports(f, model, cls.qualifiedName);

	emitDartDoc(f, cls.doc, "");
	std::string baseDart =
		cls.baseQualified.empty()
			? "BirdNativeObject"
			: model.classes.at(cls.baseQualified).dartName;

	f << "class " << cls.dartName << " extends " << baseDart << " {\n";
	if (cls.baseQualified.empty() && cls.hasPublicDestructor) {
		f << "  static final ffi.NativeFinalizer _finalizer =\n"
		  << "      ffi.NativeFinalizer(b." << cls.flatName
		  << "_deletePtr.cast());\n\n";
	}

	// fromHandle constructor
	if (cls.baseQualified.empty()) {
		f << "  " << cls.dartName
		  << ".fromHandle(super.handle, {super.owned = false})\n"
		  << "      : super.fromHandle(\n";
		if (cls.hasPublicDestructor) {
			f << "          finalizer: _finalizer,\n"
			  << "          deleter: b." << cls.flatName << "_delete,\n";
		}
		f << "        );\n\n";
	} else {
		// Pass through to BirdNativeObject via base; override finalizer via
		// internal constructor if needed, or delegate to super.fromHandle.
		f << "  " << cls.dartName
		  << ".fromHandle(super.handle, {super.owned = false})\n"
		  << "      : super.fromHandle();\n\n";
	}

	// Constructors -> Dart factories (prefer 0-arg constructor as unnamed factory)
	std::vector<const MethodInfo*> ctors;
	for (const auto& m : cls.methods) {
		if (m.isConstructor) ctors.push_back(&m);
	}
	// Sort so 0-arg constructor comes first if present
	std::stable_sort(ctors.begin(), ctors.end(),
					 [](const MethodInfo* a, const MethodInfo* b) {
						 return a->params.size() < b->params.size();
					 });
	for (size_t idx = 0; idx < ctors.size(); ++idx) {
		const MethodInfo& m = *ctors[idx];
		std::string factoryName = cls.dartName;
		if (idx > 0) {
			std::string suffix = m.mangledSuffix;
			if (!suffix.empty() && suffix[0] == '_') suffix = suffix.substr(1);
			if (suffix.empty()) suffix = "overload" + std::to_string(idx);
			factoryName += ".from" + suffix;
		}
		emitDartDoc(f, m.doc, "  ");
		f << "  factory " << factoryName << "("
		  << buildDartParamList(m.params, model) << ") {\n";
		f << "    return using((arena) {\n";
		std::vector<std::string> callArgs;
		emitDartCallArgs(f, m.params, callArgs, model);
		f << "      final h = b." << cls.flatName << "_new" << m.mangledSuffix
		  << "(";
		for (size_t i = 0; i < callArgs.size(); ++i) {
			if (i) f << ", ";
			f << callArgs[i];
		}
		f << ");\n";
		f << "      return " << cls.dartName
		  << ".fromHandle(h, owned: true);\n";
		f << "    });\n";
		f << "  }\n\n";
	}

	// Public fields -> Dart properties
	for (const auto& field : cls.fields) {
		EmittedType getRet = emitType(field.type, model, true);
		EmittedType setArg = emitType(field.type, model, false);
		std::string propName = safeDartIdent(field.name);
		std::string statKw = field.isStatic ? "static " : "";

		emitDartDoc(f, field.doc, "  ");
		f << "  " << statKw << getRet.dartUser << " get " << propName
		  << " {\n";
		f << "    return using((arena) {\n";
		std::vector<std::string> gArgs;
		if (getRet.isOutPtr) {
			std::string btype =
				field.type.cat == TypeCat::GlmMat3 ? "b.BMat3" : "b.BMat4";
			f << "      final out = arena<" << btype << ">();\n";
			gArgs.push_back("out");
		}
		if (!field.isStatic) gArgs.push_back("handle");
		std::string gCall = "b." + cls.flatName + "_get_" + field.name + "(";
		for (size_t i = 0; i < gArgs.size(); ++i) {
			if (i) gCall += ", ";
			gCall += gArgs[i];
		}
		gCall += ")";
		emitDartReturn(f, field.type, gCall, model);
		f << "    });\n";
		f << "  }\n\n";

		if (!field.isReadOnly) {
			f << "  " << statKw << "set " << propName << "(" << setArg.dartUser
			  << " value) {\n";
			f << "    using((arena) {\n";
			std::vector<std::string> sArgs;
			if (!field.isStatic) sArgs.push_back("handle");
			ParamInfo vp{"value", field.type, ""};
			emitDartCallArgs(f, {vp}, sArgs, model);
			f << "      b." << cls.flatName << "_set_" << field.name << "(";
			for (size_t i = 0; i < sArgs.size(); ++i) {
				if (i) f << ", ";
				f << sArgs[i];
			}
			f << ");\n";
			f << "    });\n";
			f << "  }\n\n";
		}
	}

	// Methods
	for (const auto& m : cls.methods) {
		if (m.isConstructor || m.isDestructor) continue;
		EmittedType ret = emitType(m.retType, model, true);
		std::string dartName = safeDartIdent(m.name + m.mangledSuffix);
		std::string statKw = m.isStatic ? "static " : "";

		emitDartDoc(f, m.doc, "  ");
		if (m.isOverride) f << "  @override\n";
		f << "  " << statKw << ret.dartUser << " " << dartName << "("
		  << buildDartParamList(m.params, model) << ") {\n";
		f << "    return using((arena) {\n";

		std::vector<std::string> callArgs;
		if (ret.isOutPtr) {
			std::string btype =
				m.retType.cat == TypeCat::GlmMat3 ? "b.BMat3" : "b.BMat4";
			f << "      final out = arena<" << btype << ">();\n";
			callArgs.push_back("out");
		}
		if (!m.isStatic) callArgs.push_back("handle");
		emitDartCallArgs(f, m.params, callArgs, model);

		std::string callExpr =
			"b." + cls.flatName + "_" + m.name + m.mangledSuffix + "(";
		for (size_t i = 0; i < callArgs.size(); ++i) {
			if (i) callExpr += ", ";
			callExpr += callArgs[i];
		}
		callExpr += ")";
		emitDartReturn(f, m.retType, callExpr, model);
		f << "    });\n";
		f << "  }\n\n";
	}

	f << "}\n";
}

static void writeFreeFunctionsFile(const fs::path& outDir, const Model& model) {
	std::ofstream f(outDir / "functions.dart");
	f << "// AUTO-GENERATED by DartApiGenerator. Do not edit by hand.\n";
	f << "// Free functions in namespace bird.\n\n";
	writeCommonImports(f, model);

	for (const auto& fn : model.freeFunctions) {
		EmittedType ret = emitType(fn.retType, model, true);
		std::string dartName = safeDartIdent(fn.simpleName + fn.mangledSuffix);
		emitDartDoc(f, fn.doc, "");
		f << ret.dartUser << " " << dartName << "("
		  << buildDartParamList(fn.params, model) << ") {\n";
		f << "  return using((arena) {\n";
		std::vector<std::string> callArgs;
		if (ret.isOutPtr) {
			std::string btype =
				fn.retType.cat == TypeCat::GlmMat3 ? "b.BMat3" : "b.BMat4";
			f << "    final out = arena<" << btype << ">();\n";
			callArgs.push_back("out");
		}
		emitDartCallArgs(f, fn.params, callArgs, model);
		std::string callExpr =
			"b.bird_fn_" + fn.simpleName + fn.mangledSuffix + "(";
		for (size_t i = 0; i < callArgs.size(); ++i) {
			if (i) callExpr += ", ";
			callExpr += callArgs[i];
		}
		callExpr += ")";
		emitDartReturn(f, fn.retType, callExpr, model);
		f << "  });\n";
		f << "}\n\n";
	}
}

static void writeGlobalsFile(const fs::path& outDir, const Model& model) {
	std::ofstream f(outDir / "globals.dart");
	f << "// AUTO-GENERATED by DartApiGenerator. Do not edit by hand.\n";
	f << "// Global engine singletons.\n\n";
	writeCommonImports(f, model);

	for (const auto& [_, g] : model.singletons) {
		std::string targetDart = model.classes.at(g.targetClass).dartName;
		emitDartDoc(f, g.doc, "");
		f << targetDart << "? get " << g.dartName << " {\n";
		f << "  final p = b.bird_global_" << g.simpleName << "();\n";
		f << "  if (p == ffi.nullptr) return null;\n";
		f << "  return " << targetDart << ".fromHandle(p, owned: false);\n";
		f << "}\n\n";
	}
}

static void writeBarrelFile(const fs::path& outDir, const Model& model) {
	std::ofstream f(outDir / "generated.dart");
	f << "// AUTO-GENERATED by DartApiGenerator. Do not edit by hand.\n";
	f << "// Barrel export for all generated Bird Engine Dart bindings.\n\n";
	f << "export 'bindings.dart' show setNativeLibrary, nativeLib;\n";
	f << "export 'enums.dart';\n";
	f << "export 'functions.dart';\n";
	f << "export 'globals.dart';\n";
	f << "export 'input_constants.dart';\n";
	f << "export 'math.dart';\n";
	f << "export 'native_object.dart';\n";
	f << "export 'shared_refs.dart';\n";
	for (const auto& [_, cls] : model.classes) {
		f << "export '" << toSnakeCase(cls.dartName) << ".dart';\n";
	}
}

static void writeFiles(const fs::path& outDir, const Model& model) {
	fs::create_directories(outDir);
	writeNativeObjectBase(outDir);
	writeMathFile(outDir);
	writeEnumsFile(outDir, model);
	writeSharedRefsFile(outDir, model);
	writeInputConstantsFile(outDir, model);
	for (const auto& [_, cls] : model.classes) {
		writeClassFile(outDir, cls, model);
	}
	writeFreeFunctionsFile(outDir, model);
	writeGlobalsFile(outDir, model);
	writeBarrelFile(outDir, model);
}

}  // namespace dproxy

// =============================================================================
// 10. CLI & Driver
// =============================================================================

struct CliArgs {
	fs::path outC;
	fs::path outDart;
	std::string libName = "libBird_core";
	std::vector<fs::path> headers;
	std::vector<std::string> clangArgs;
};

static void printUsage() {
	std::cerr
		<< "Usage: DartApiGenerator --out-c <dir> --out-dart <dir> "
		   "[--lib <name>] [<clang flags...> --] <header>...\n";
}

static bool parseArgs(int argc, char** argv, CliArgs& out) {
	bool sawDoubleDash = false;
	for (int i = 1; i < argc; ++i) {
		std::string a = argv[i];
		if (sawDoubleDash) {
			fs::path p(a);
			if (fs::exists(p) && !fs::is_directory(p))
				out.headers.push_back(p);
			else
				out.clangArgs.push_back(a);
			continue;
		}
		if (a == "--out" && i + 1 < argc) {
			fs::path d = argv[++i];
			out.outC = d;
			out.outDart = d;
		} else if (a == "--out-c" && i + 1 < argc) {
			out.outC = argv[++i];
		} else if (a == "--out-dart" && i + 1 < argc) {
			out.outDart = argv[++i];
		} else if (a == "--lib" && i + 1 < argc) {
			out.libName = argv[++i];
		} else if (a == "--") {
			sawDoubleDash = true;
		} else if (a == "-h" || a == "--help") {
			printUsage();
			std::exit(0);
		} else {
			fs::path p(a);
			if (fs::exists(p) && !fs::is_directory(p))
				out.headers.push_back(p);
			else
				out.clangArgs.push_back(a);
		}
	}
	if (out.outC.empty() || out.outDart.empty() || out.headers.empty()) {
		printUsage();
		return false;
	}
	return true;
}

int main(int argc, char** argv) {
	CliArgs cli;
	if (!parseArgs(argc, argv, cli)) return 1;

	Model model;
	for (const auto& h : cli.headers) {
		std::error_code ec;
		model.targetHeaderCanonicalPaths.insert(
			fs::weakly_canonical(h, ec).string());
	}

	CXIndex index = clang_createIndex(0, 1);

	std::vector<const char*> args;
	args.push_back("-x");
	args.push_back("c++");
	args.push_back("-std=c++17");
	args.push_back("-DBIRD_CODEGEN");
	for (const auto& a : cli.clangArgs) args.push_back(a.c_str());

	VisitorState state;
	state.model = &model;
	state.nsFilter = {"bird"};

	for (const auto& header : cli.headers) {
		CXTranslationUnit tu = nullptr;
		CXErrorCode ec = clang_parseTranslationUnit2(
			index, header.string().c_str(), args.data(),
			static_cast<int>(args.size()), nullptr, 0,
			CXTranslationUnit_SkipFunctionBodies, &tu);
		if (ec != CXError_Success || !tu) {
			std::cerr << "[DartApiGenerator] error: failed to parse " << header
					  << "\n";
			clang_disposeIndex(index);
			return 1;
		}
		CXCursor root = clang_getTranslationUnitCursor(tu);
		clang_visitChildren(root, visitTU, &state);
		clang_disposeTranslationUnit(tu);
	}
	clang_disposeIndex(index);

	extractInputMacroConstants(cli.headers, model);
	finalizeModel(model);

	cwrap::writeFiles(cli.outC, cli.headers, model);
	dffi::writeFile(cli.outDart, cli.libName, model);
	dproxy::writeFiles(cli.outDart, model);

	std::cout << "[DartApiGenerator] generated bindings for "
			  << model.classes.size() << " classes, " << model.enums.size()
			  << " enums, " << model.freeFunctions.size()
			  << " free functions, " << model.singletons.size()
			  << " singletons, " << model.macroConstants.size()
			  << " input constants.\n";
	return 0;
}
