
// =============================================================================
// DartApiGenerator.cpp
// -----------------------------------------------------------------------------
// Parses C++ engine headers with libclang and emits a three-tier interop layer
// for Dart scripting:
//
//   Tier 1: A flat C wrapper (extern "C") — bypasses C++ name mangling.
//           Files:  c_wrapper.h, c_wrapper.cpp
//
//   Tier 2: Low-level Dart FFI bindings (dart:ffi) — Pointer<NativeFunction>
//           lookups and FFI struct definitions for GLM types.
//           File:   bindings.dart
//
//   Tier 3: High-level Dart proxy classes that mirror the original C++ API
//           and own the underlying C++ instance via a NativeFinalizer.
//           Files:  one <class>.dart per exported class, plus math.dart
//
// Build:  see CMakeLists.txt — links against libclang.
//
// Usage:
//   DartApiGenerator --out <dir> [--lib <name>] [-- <clang args...>]
//   <header>...
//
// Example:
//   DartApiGenerator --out gen/dart --lib libbird_engine -I src/core -- \
//       src/core/scene/Scene.h src/core/scene/WorldObject.h
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
#include <set>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace fs = std::filesystem;

// =============================================================================
// 0. Small utilities
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

// "bird::scene::WorldObject" -> "bird_scene_WorldObject"
static std::string flattenNs(std::string s) {
	return replaceAll(std::move(s), "::", "_");
}

// Recover the fully-qualified name of a cursor by walking semantic parents.
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

static void warn(const std::string& msg) {
	std::cerr << "[warn] " << msg << "\n";
}

// =============================================================================
// 1. Type system
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
	CString,  // (const) char*
	GlmVec2,
	GlmVec3,
	GlmVec4,
	GlmQuat,
	GlmMat4,
	OpaqueClass,  // user-defined class -> Pointer<Void>
	Unsupported
};

struct TypeInfo {
	TypeCat cat = TypeCat::Unsupported;
	std::string cppRaw;		   // "const bird::Scene*"
	std::string cppCanonical;  // "bird::Scene" (no ptr/ref/cv)
	bool isConst = false;
	bool isPointer = false;
	bool isReference = false;
	// For GlmMat4 (large struct) and OpaqueClass we always pass via pointer in
	// C. For small GLM structs we pass by value.
};

// Strip cv-qualifiers and a single ptr/ref level from a type spelling.
// Used as a fallback when libclang's canonical type string isn't ideal.
static std::string stripCvPtrRef(std::string s) {
	s = trim(s);
	while (startsWith(s, "const ")) {
		s = s.substr(6);
		s = trim(s);
	}
	while (!s.empty() && (s.back() == '*' || s.back() == '&')) {
		s.pop_back();
		s = trim(s);
		if (!s.empty() && s.size() >= 5 && s.substr(s.size() - 5) == "const")
			s = trim(s.substr(0, s.size() - 5));
	}
	return s;
}

static TypeInfo translateType(CXType t) {
	TypeInfo info;
	info.cppRaw = typeSpelling(t);
	info.isConst = clang_isConstQualifiedType(t);

	// Peel pointer/reference layers.
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

	// const char* -> CString (only when there's a pointer level)
	if (info.isPointer && (canonStr == "char" || canonStr == "signed char")) {
		info.cat = TypeCat::CString;
		return info;
	}

	// Primitives (only when not pointer/ref)
	if (!info.isPointer && !info.isReference) {
		switch (canon.kind) {
			case CXType_Void:
				info.cat = TypeCat::Void;
				return info;
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
			default:
				break;
		}
	}

	// GLM types — match by canonical name. They live in namespace glm.
	if (canonStr == "glm::vec2") {
		info.cat = TypeCat::GlmVec2;
		return info;
	}
	if (canonStr == "glm::vec3") {
		info.cat = TypeCat::GlmVec3;
		return info;
	}
	if (canonStr == "glm::vec4") {
		info.cat = TypeCat::GlmVec4;
		return info;
	}
	if (canonStr == "glm::quat") {
		info.cat = TypeCat::GlmQuat;
		return info;
	}
	if (canonStr == "glm::mat4") {
		info.cat = TypeCat::GlmMat4;
		return info;
	}

	// Anything else that resolves to a record we treat as an opaque class.
	if (canon.kind == CXType_Record || canon.kind == CXType_Unexposed ||
		canon.kind == CXType_Elaborated) {
		info.cat = TypeCat::OpaqueClass;
		return info;
	}

	info.cat = TypeCat::Unsupported;
	return info;
}

// -----------------------------------------------------------------------------
// Type → emitted strings for each tier.
// -----------------------------------------------------------------------------

struct EmittedType {
	std::string cType;	// C wrapper signature, e.g. "void*", "BVec3", "float"
	std::string ffiNative;	// dart:ffi Native* type, e.g.
							// "ffi.Pointer<ffi.Void>", "BVec3", "ffi.Float"
	std::string ffiDart;	// dart-side function-signature type, e.g.
							// "ffi.Pointer<ffi.Void>", "BVec3", "double"
	std::string dartUser;	// user-facing dart type, e.g. "WorldObject",
							// "Vector3", "double", "bool"
	bool valid = true;
	bool isLargeGlm = false;  // mat4 — passed by const*, returned via out-ptr
};

// Dart-side friendly names. We keep the same conventions as bird::Vector3
// typedefs.
static std::string glmDartName(TypeCat c) {
	switch (c) {
		case TypeCat::GlmVec2:
			return "Vector2";
		case TypeCat::GlmVec3:
			return "Vector3";
		case TypeCat::GlmVec4:
			return "Vector4";
		case TypeCat::GlmQuat:
			return "Quaternion";
		case TypeCat::GlmMat4:
			return "Matrix4";
		default:
			return "";
	}
}

static std::string glmCName(TypeCat c) {
	switch (c) {
		case TypeCat::GlmVec2:
			return "BVec2";
		case TypeCat::GlmVec3:
			return "BVec3";
		case TypeCat::GlmVec4:
			return "BVec4";
		case TypeCat::GlmQuat:
			return "BQuat";
		case TypeCat::GlmMat4:
			return "BMat4";
		default:
			return "";
	}
}

// Last simple identifier — turns "bird::scene::WorldObject" into "WorldObject".
static std::string lastIdent(const std::string& qual) {
	auto p = qual.rfind("::");
	return p == std::string::npos ? qual : qual.substr(p + 2);
}

static EmittedType emitForReturn(const TypeInfo& t) {
	EmittedType e;
	switch (t.cat) {
		case TypeCat::Void:
			e.cType = "void";
			e.ffiNative = "ffi.Void";
			e.ffiDart = "void";
			e.dartUser = "void";
			return e;
		case TypeCat::Bool:
			e.cType = "bool";
			e.ffiNative = "ffi.Bool";
			e.ffiDart = "bool";
			e.dartUser = "bool";
			return e;
		case TypeCat::Int32:
			e.cType = "int32_t";
			e.ffiNative = "ffi.Int32";
			e.ffiDart = "int";
			e.dartUser = "int";
			return e;
		case TypeCat::UInt32:
			e.cType = "uint32_t";
			e.ffiNative = "ffi.Uint32";
			e.ffiDart = "int";
			e.dartUser = "int";
			return e;
		case TypeCat::Int64:
			e.cType = "int64_t";
			e.ffiNative = "ffi.Int64";
			e.ffiDart = "int";
			e.dartUser = "int";
			return e;
		case TypeCat::UInt64:
			e.cType = "uint64_t";
			e.ffiNative = "ffi.Uint64";
			e.ffiDart = "int";
			e.dartUser = "int";
			return e;
		case TypeCat::Float:
			e.cType = "float";
			e.ffiNative = "ffi.Float";
			e.ffiDart = "double";
			e.dartUser = "double";
			return e;
		case TypeCat::Double:
			e.cType = "double";
			e.ffiNative = "ffi.Double";
			e.ffiDart = "double";
			e.dartUser = "double";
			return e;
		case TypeCat::CString:
			e.cType = "const char*";
			e.ffiNative = "ffi.Pointer<ffi.Utf8>";
			e.ffiDart = "ffi.Pointer<ffi.Utf8>";
			e.dartUser = "String";
			return e;

		case TypeCat::GlmVec2:
		case TypeCat::GlmVec3:
		case TypeCat::GlmVec4:
		case TypeCat::GlmQuat: {
			std::string cn = glmCName(t.cat), dn = glmDartName(t.cat);
			e.cType = cn;
			e.ffiNative = cn;
			e.ffiDart = cn;
			e.dartUser = dn;
			return e;
		}
		case TypeCat::GlmMat4:
			// mat4 returns: void func(BMat4* out, ...) on the C side.
			// Emit the user-facing "Matrix4" return type; codegen for body
			// handles the out-pointer plumbing.
			e.cType = "void";
			e.ffiNative = "ffi.Void";
			e.ffiDart = "void";
			e.dartUser = "Matrix4";
			e.isLargeGlm = true;
			return e;

		case TypeCat::OpaqueClass:
			// All opaque classes round-trip as void*.
			e.cType = "void*";
			e.ffiNative = "ffi.Pointer<ffi.Void>";
			e.ffiDart = "ffi.Pointer<ffi.Void>";
			e.dartUser = lastIdent(t.cppCanonical);
			return e;

		case TypeCat::Unsupported:
		default:
			e.valid = false;
			return e;
	}
}

static EmittedType emitForParam(const TypeInfo& t) {
	// Same as return mapping except mat4 must be passed as const BMat4*.
	EmittedType e = emitForReturn(t);
	if (t.cat == TypeCat::GlmMat4) {
		e.cType = "const BMat4*";
		e.ffiNative = "ffi.Pointer<BMat4>";
		e.ffiDart = "ffi.Pointer<BMat4>";
		e.dartUser = "Matrix4";
		e.isLargeGlm = true;
	}
	return e;
}

// =============================================================================
// 2. Intermediate representation
// =============================================================================

struct ParamInfo {
	std::string name;  // may be empty
	TypeInfo type;
	std::string defaultExpr;  // raw token text if any, else empty
};

struct MethodInfo {
	std::string name;  // unmangled method name as written, e.g. "translate"
	std::string mangledSuffix;	// suffix appended for overload disambiguation,
								// e.g. "_Vec3"
	bool isStatic = false;
	bool isVirtual = false;
	bool isPureVirtual = false;
	bool isConst = false;
	TypeInfo retType;
	std::vector<ParamInfo> params;
	bool isConstructor = false;
	bool isDestructor = false;
};

struct FieldInfo {
	std::string name;
	TypeInfo type;
	bool isPublic = true;
};

struct ClassInfo {
	std::string qualifiedName;	// "bird::Scene"
	std::string flatName;		// "bird_Scene"
	std::string simpleName;		// "Scene"
	std::vector<MethodInfo> methods;
	std::vector<FieldInfo>
		fields;	 // public fields only, currently unused in proxy
	bool hasUserConstructor = false;
	bool hasVisibleDestructor = true;  // assumed unless deleted/private
};

// =============================================================================
// 3. AST visitor
// =============================================================================

struct VisitorState {
	std::map<std::string, ClassInfo> classes;  // keyed by qualifiedName
	const std::set<std::string>* nsFilter = nullptr;
	fs::path tuFile;
};

// Decide whether a class is in scope for export.
static bool inScope(const std::string& qualifiedName,
					const std::set<std::string>& nsFilter) {
	if (nsFilter.empty()) return true;
	for (auto& ns : nsFilter) {
		if (startsWith(qualifiedName, ns + "::")) return true;
	}
	return false;
}

// Try to extract a simple default-argument expression as raw token text.
// Returns empty string if nothing was found or it's too complex to be safe.
static std::string extractDefaultExpr(CXCursor paramCursor) {
	std::string out;
	clang_visitChildren(
		paramCursor,
		[](CXCursor c, CXCursor, CXClientData data) {
			// Any expression child of a ParmDecl is its default initializer.
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

// Visitor for class body — collects methods and public fields.
static CXChildVisitResult visitClassBody(CXCursor c, CXCursor /*parent*/,
										 CXClientData data) {
	auto* cls = static_cast<ClassInfo*>(data);
	CXCursorKind kind = clang_getCursorKind(c);

	// Respect access specifier — skip non-public members.
	CX_CXXAccessSpecifier acc = clang_getCXXAccessSpecifier(c);
	if (acc == CX_CXXPrivate || acc == CX_CXXProtected)
		return CXChildVisit_Continue;

	switch (kind) {
		case CXCursor_CXXMethod:
		case CXCursor_Constructor:
		case CXCursor_Destructor: {
			MethodInfo m = buildMethod(c);

			// Drop pure-virtuals: the proxy can't instantiate an abstract class
			// and concrete C++ implementations should be exposed instead.
			if (m.isPureVirtual) {
				warn("skipping pure-virtual " + cls->qualifiedName +
					 "::" + m.name);
				return CXChildVisit_Continue;
			}
			// Drop methods with unsupported parameter or return types.
			bool ok = (m.retType.cat != TypeCat::Unsupported);
			for (auto& p : m.params)
				ok = ok && (p.type.cat != TypeCat::Unsupported);
			if (!ok) {
				warn("skipping " + cls->qualifiedName + "::" + m.name +
					 " — unsupported type in signature");
				return CXChildVisit_Continue;
			}
			// Skip operators (could be added later).
			if (startsWith(m.name, "operator")) return CXChildVisit_Continue;

			if (m.isConstructor) cls->hasUserConstructor = true;
			cls->methods.push_back(std::move(m));
			return CXChildVisit_Continue;
		}
		case CXCursor_FieldDecl: {
			FieldInfo f;
			f.name = cursorSpelling(c);
			f.type = translateType(clang_getCursorType(c));
			f.isPublic = (acc == CX_CXXPublic);
			if (f.isPublic && f.type.cat != TypeCat::Unsupported)
				cls->fields.push_back(std::move(f));
			return CXChildVisit_Continue;
		}
		default:
			return CXChildVisit_Continue;
	}
}

// Top-level visitor — discovers classes/structs and recurses into them.
static CXChildVisitResult visitTU(CXCursor c, CXCursor /*parent*/,
								  CXClientData data) {
	auto* st = static_cast<VisitorState*>(data);

	// Stay in the user's source files — skip system headers.
	CXSourceLocation loc = clang_getCursorLocation(c);
	if (clang_Location_isInSystemHeader(loc)) return CXChildVisit_Continue;

	CXCursorKind kind = clang_getCursorKind(c);

	if (kind == CXCursor_Namespace) {
		// Recurse into namespaces unconditionally — filtering happens
		// per-class.
		return CXChildVisit_Recurse;
	}

	if (kind == CXCursor_ClassDecl || kind == CXCursor_StructDecl) {
		// Definitions only — forward decls are not exportable.
		if (!clang_isCursorDefinition(c)) return CXChildVisit_Continue;

		std::string qn = fullyQualifiedName(c);
		if (!inScope(qn, *st->nsFilter)) return CXChildVisit_Continue;

		ClassInfo cls;
		cls.qualifiedName = qn;
		cls.flatName = flattenNs(qn);
		cls.simpleName = lastIdent(qn);

		clang_visitChildren(c, visitClassBody, &cls);

		// De-duplicate against earlier visits of the same class (multiple TUs).
		auto it = st->classes.find(qn);
		if (it == st->classes.end()) st->classes.emplace(qn, std::move(cls));
		return CXChildVisit_Continue;
	}

	return CXChildVisit_Recurse;
}

// =============================================================================
// 4. Overload disambiguation
// =============================================================================
//
// For each class, methods that share a name receive a suffix derived from
// their parameter types. If a name has only one definition, no suffix is
// applied. The suffix uses short tokens so it stays readable:
//   void / bool / i / u / i64 / u64 / f / d / str
//   Vec2 / Vec3 / Vec4 / Quat / Mat4
//   <SimpleClassName> for opaque classes (drops namespace).

static std::string paramTag(const TypeInfo& t) {
	switch (t.cat) {
		case TypeCat::Bool:
			return "bool";
		case TypeCat::Int32:
			return "i";
		case TypeCat::UInt32:
			return "u";
		case TypeCat::Int64:
			return "i64";
		case TypeCat::UInt64:
			return "u64";
		case TypeCat::Float:
			return "f";
		case TypeCat::Double:
			return "d";
		case TypeCat::CString:
			return "str";
		case TypeCat::GlmVec2:
			return "Vec2";
		case TypeCat::GlmVec3:
			return "Vec3";
		case TypeCat::GlmVec4:
			return "Vec4";
		case TypeCat::GlmQuat:
			return "Quat";
		case TypeCat::GlmMat4:
			return "Mat4";
		case TypeCat::OpaqueClass:
			return lastIdent(t.cppCanonical);
		default:
			return "x";
	}
}

static void disambiguateOverloads(ClassInfo& cls) {
	std::map<std::string, int> counts;
	for (auto& m : cls.methods) counts[m.name]++;
	for (auto& m : cls.methods) {
		if (counts[m.name] <= 1) continue;
		std::string s;
		for (auto& p : m.params) {
			s += "_";
			s += paramTag(p.type);
		}
		if (s.empty()) s = "_void";
		m.mangledSuffix = s;
	}
}

// =============================================================================
// 5. C wrapper generation
// =============================================================================

namespace cwrap {

// e.g. "bird_WorldObject_translate" or "bird_WorldObject_setPosition_Vec3"
static std::string fnName(const ClassInfo& c, const MethodInfo& m,
						  const std::string& tag) {
	std::string n = c.flatName + "_";
	if (m.isConstructor)
		n += "new";
	else if (m.isDestructor)
		n += "delete";
	else
		n += m.name;
	if (!tag.empty()) n += tag;
	return n;
}

static std::string ctorName(const ClassInfo& c) {
	return c.flatName + "_new";
}
static std::string dtorName(const ClassInfo& c) {
	return c.flatName + "_delete";
}

// Emit a single function declaration into both header and impl.
static void emitFn(std::ostream& hdr, std::ostream& src, const ClassInfo& cls,
				   const MethodInfo& m) {
	EmittedType ret = emitForReturn(m.retType);
	if (!ret.valid) return;

	std::string name = fnName(cls, m, m.mangledSuffix);

	// Build parameter list shared by header decl and impl signature.
	std::string params;
	bool first = true;
	auto addParam = [&](const std::string& type, const std::string& pname) {
		if (!first) params += ", ";
		params += type + " " + pname;
		first = false;
	};

	// mat4 return: prepend out-pointer.
	if (ret.isLargeGlm) addParam("BMat4*", "out_result");

	// Instance methods take 'this' as first real arg.
	bool isInstance = !m.isStatic && !m.isConstructor;
	if (isInstance) addParam("void*", "self");

	// Regular params.
	for (auto& p : m.params) {
		EmittedType pe = emitForParam(p.type);
		addParam(pe.cType, p.name);
	}
	if (params.empty()) params = "void";

	// ----- Header declaration -----
	if (m.isConstructor) {
		hdr << "void* " << name << "(" << params << ");\n";
	} else if (m.isDestructor) {
		// We always emit a deleter regardless of whether the dtor was
		// traversed, so skip duplicate emission here.
		return;
	} else {
		hdr << ret.cType << " " << name << "(" << params << ");\n";
	}

	// ----- Implementation -----
	if (m.isConstructor) {
		src << "void* " << name << "(" << params << ") {\n";
		src << "    return new ::" << cls.qualifiedName << "(";
		for (size_t i = 0; i < m.params.size(); ++i) {
			if (i) src << ", ";
			const auto& p = m.params[i];
			switch (p.type.cat) {
				case TypeCat::GlmVec2:
				case TypeCat::GlmVec3:
				case TypeCat::GlmVec4:
				case TypeCat::GlmQuat:
					src << "to_glm(" << p.name << ")";
					break;
				case TypeCat::GlmMat4:
					src << "to_glm(*" << p.name << ")";
					break;
				case TypeCat::OpaqueClass:
					src << "static_cast<::" << p.type.cppCanonical
						<< (p.type.isPointer ? "*>(" : "*>(") << p.name << ")";
					if (p.type.isReference) src << " /* deref */";
					break;
				case TypeCat::CString:
					src << p.name;
					break;
				default:
					src << p.name;
					break;
			}
		}
		src << ");\n}\n\n";
		return;
	}

	src << ret.cType << " " << name << "(" << params << ") {\n";
	if (isInstance) {
		std::string constness = m.isConst ? "const " : "";
		src << "    auto* _this = static_cast<" << constness
			<< "::" << cls.qualifiedName << "*>(self);\n";
	}

	// Build the call expression.
	std::ostringstream call;
	if (isInstance)
		call << "_this->" << m.name << "(";
	else if (m.isStatic)
		call << "::" << cls.qualifiedName << "::" << m.name << "(";
	for (size_t i = 0; i < m.params.size(); ++i) {
		if (i) call << ", ";
		const auto& p = m.params[i];
		switch (p.type.cat) {
			case TypeCat::GlmVec2:
			case TypeCat::GlmVec3:
			case TypeCat::GlmVec4:
			case TypeCat::GlmQuat:
				call << "to_glm(" << p.name << ")";
				break;
			case TypeCat::GlmMat4:
				call << "to_glm(*" << p.name << ")";
				break;
			case TypeCat::OpaqueClass:
				call << "*static_cast<::" << p.type.cppCanonical << "*>("
					 << p.name << ")";
				// If parameter is taken by pointer rather than ref, remove
				// deref.
				if (p.type.isPointer) {
					call.str("");
					call.clear();
					// Rebuild: easier path — recompose.
				}
				break;
			case TypeCat::CString:
				call << p.name;
				break;
			default:
				call << p.name;
				break;
		}
		// Pointer-to-class needs a different shape than reference-to-class.
		if (p.type.cat == TypeCat::OpaqueClass && p.type.isPointer) {
			// overwrite the last argument we just added by replaying with the
			// correct shape — we keep this branch separate for clarity.
			// (For simplicity, always re-emit the arg from scratch when
			// class+ptr.)
			std::string fixed;
			if (i) fixed = ", ";
			fixed += "static_cast<::";
			fixed += p.type.cppCanonical;
			fixed += "*>(";
			fixed += p.name;
			fixed += ")";
			// Replace the tail of the stream content corresponding to this arg.
			std::string s = call.str();
			// Strip the previously appended (and possibly malformed) section
			// by truncating to the prior argument boundary.
			size_t lastComma = s.find_last_of(',');
			if (lastComma == std::string::npos) {
				size_t lparen = s.find_last_of('(');
				s = s.substr(0, lparen + 1);
			} else {
				s = s.substr(0, lastComma);
			}
			call.str("");
			call.clear();
			call << s << fixed;
		}
	}
	call << ")";

	// Apply the result.
	switch (m.retType.cat) {
		case TypeCat::Void:
			src << "    " << call.str() << ";\n";
			break;
		case TypeCat::GlmVec2:
		case TypeCat::GlmVec3:
		case TypeCat::GlmVec4:
		case TypeCat::GlmQuat:
			src << "    return from_glm(" << call.str() << ");\n";
			break;
		case TypeCat::GlmMat4:
			src << "    *out_result = from_glm(" << call.str() << ");\n";
			break;
		case TypeCat::OpaqueClass:
			// C++ may return by value, by ref, or by pointer. We always hand
			// back a void*. For by-value returns this is unsafe (returning
			// address of a temporary) so we skip those upstream.
			if (m.retType.isPointer) {
				src << "    return static_cast<void*>(" << call.str() << ");\n";
			} else if (m.retType.isReference) {
				src << "    return static_cast<void*>(&(" << call.str()
					<< "));\n";
			} else {
				src << "    // WARN: returning class by value is not yet "
					   "supported.\n";
				src << "    return nullptr;\n";
			}
			break;
		case TypeCat::CString:
			src << "    return " << call.str() << ";\n";
			break;
		default:
			src << "    return " << call.str() << ";\n";
	}
	src << "}\n\n";
}

static void writeFiles(const fs::path& outDir,
					   const std::vector<fs::path>& sourceHeaders,
					   const std::map<std::string, ClassInfo>& classes) {
	fs::create_directories(outDir);

	std::ofstream hdr(outDir / "c_wrapper.h");
	std::ofstream src(outDir / "c_wrapper.cpp");

	// ---------- header ----------
	hdr << "// AUTO-GENERATED by DartApiGenerator. Do not edit by hand.\n";
	hdr << "#pragma once\n";
	hdr << "#include <stdint.h>\n";
	hdr << "#include <stdbool.h>\n\n";
	hdr << "#ifdef __cplusplus\nextern \"C\" {\n#endif\n\n";

	hdr << "// ---- GLM POD mirrors ----\n";
	hdr << "typedef struct BVec2 { float x, y; } BVec2;\n";
	hdr << "typedef struct BVec3 { float x, y, z; } BVec3;\n";
	hdr << "typedef struct BVec4 { float x, y, z, w; } BVec4;\n";
	hdr << "typedef struct BQuat { float x, y, z, w; } BQuat;\n";
	hdr << "typedef struct BMat4 { float m[16]; } BMat4;\n\n";

	// ---------- impl ----------
	src << "// AUTO-GENERATED by DartApiGenerator. Do not edit by hand.\n";
	src << "#include \"c_wrapper.h\"\n";
	src << "#include <glm/glm.hpp>\n";
	src << "#include <glm/gtc/quaternion.hpp>\n";
	src << "#include <glm/gtc/type_ptr.hpp>\n";
	for (auto& h : sourceHeaders) src << "#include \"" << h.string() << "\"\n";
	src << "\n";
	src << "// GLM <-> POD adapters.\n";
	src << "static inline glm::vec2 to_glm(BVec2 v){ return {v.x,v.y}; }\n";
	src << "static inline glm::vec3 to_glm(BVec3 v){ return {v.x,v.y,v.z}; }\n";
	src << "static inline glm::vec4 to_glm(BVec4 v){ return {v.x,v.y,v.z,v.w}; "
		   "}\n";
	src << "static inline glm::quat to_glm(BQuat q){ return "
		   "glm::quat(q.w,q.x,q.y,q.z); }\n";
	src << "static inline glm::mat4 to_glm(const BMat4& m){\n";
	src << "    glm::mat4 r; for(int i=0;i<16;++i) "
		   "glm::value_ptr(r)[i]=m.m[i]; return r;\n";
	src << "}\n";
	src << "static inline BVec2 from_glm(const glm::vec2& v){ return "
		   "{v.x,v.y}; }\n";
	src << "static inline BVec3 from_glm(const glm::vec3& v){ return "
		   "{v.x,v.y,v.z}; }\n";
	src << "static inline BVec4 from_glm(const glm::vec4& v){ return "
		   "{v.x,v.y,v.z,v.w}; }\n";
	src << "static inline BQuat from_glm(const glm::quat& q){ return "
		   "{q.x,q.y,q.z,q.w}; }\n";
	src << "static inline BMat4 from_glm(const glm::mat4& m){\n";
	src << "    BMat4 r; for(int i=0;i<16;++i) r.m[i]=glm::value_ptr(m)[i]; "
		   "return r;\n";
	src << "}\n\n";
	src << "extern \"C\" {\n\n";

	for (auto& [_, cls] : classes) {
		hdr << "// ===== " << cls.qualifiedName << " =====\n";
		// Always emit a destroyer that NativeFinalizer can use.
		hdr << "void " << dtorName(cls) << "(void* self);\n";

		src << "// ===== " << cls.qualifiedName << " =====\n";
		src << "void " << dtorName(cls)
			<< "(void* self) { delete static_cast<::" << cls.qualifiedName
			<< "*>(self); }\n\n";

		for (auto& m : cls.methods) {
			if (m.isDestructor) continue;  // emitted above
			emitFn(hdr, src, cls, m);
		}
		hdr << "\n";
	}

	hdr << "#ifdef __cplusplus\n}\n#endif\n";
	src << "} // extern \"C\"\n";
}

}  // namespace cwrap

// =============================================================================
// 6. Dart FFI bindings generation  (bindings.dart)
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

static void writeFile(const fs::path& outDir, const std::string& libName,
					  const std::map<std::string, ClassInfo>& classes) {
	std::ofstream f(outDir / "bindings.dart");
	f << "// AUTO-GENERATED by DartApiGenerator. Do not edit by hand.\n";
	f << "// Low-level FFI bindings.\n\n";
	f << "import 'dart:ffi' as ffi;\n";
	f << "import 'dart:io' show Platform;\n";
	f << "import 'package:ffi/ffi.dart';\n\n";

	// --- Library handle ---
	f << "final ffi.DynamicLibrary _dylib = _open();\n";
	f << "ffi.DynamicLibrary _open() {\n";
	f << "  if (Platform.isWindows) return ffi.DynamicLibrary.open('" << libName
	  << ".dll');\n";
	f << "  if (Platform.isMacOS)   return ffi.DynamicLibrary.open('" << libName
	  << ".dylib');\n";
	f << "  return ffi.DynamicLibrary.open('" << libName << ".so');\n";
	f << "}\n";
	f << "ffi.DynamicLibrary get nativeLib => _dylib;\n\n";

	// --- GLM POD structs ---
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
	f << "final class BMat4 extends ffi.Struct {\n"
	  << "  @ffi.Array.multi([16]) external ffi.Array<ffi.Float> m;\n}\n\n";

	// --- Per-class function lookups ---
	for (auto& [_, cls] : classes) {
		f << "// ===== " << cls.qualifiedName << " =====\n";

		// Destroyer — must use Void Function(Pointer<Void>) shape exactly so it
		// can be passed to NativeFinalizer.
		std::string dtorSym = cls.flatName + "_delete";
		f << "final ffi.Pointer<ffi.NativeFunction<ffi.Void "
			 "Function(ffi.Pointer<ffi.Void>)>>\n"
		  << "    " << dtorSym << "Ptr =\n"
		  << "        _dylib.lookup<ffi.NativeFunction<ffi.Void "
			 "Function(ffi.Pointer<ffi.Void>)>>"
		  << "('" << dtorSym << "');\n";
		f << "final void Function(ffi.Pointer<ffi.Void>) " << dtorSym << " = "
		  << dtorSym << "Ptr.asFunction();\n\n";

		for (auto& m : cls.methods) {
			if (m.isDestructor) continue;

			EmittedType ret = emitForReturn(m.retType);
			if (!ret.valid) continue;

			std::string sym = cls.flatName + "_" +
							  (m.isConstructor ? "new" : m.name) +
							  m.mangledSuffix;

			// Native + Dart parameter type lists.
			std::vector<std::string> nativeArgs, dartArgs;
			std::string nativeRet, dartRet;
			if (m.isConstructor) {
				nativeRet = "ffi.Pointer<ffi.Void>";
				dartRet = "ffi.Pointer<ffi.Void>";
			} else {
				nativeRet = ret.ffiNative;
				dartRet = ret.ffiDart;
			}

			if (ret.isLargeGlm) {
				nativeArgs.push_back("ffi.Pointer<BMat4>");
				dartArgs.push_back("ffi.Pointer<BMat4>");
			}
			if (!m.isStatic && !m.isConstructor) {
				nativeArgs.push_back("ffi.Pointer<ffi.Void>");
				dartArgs.push_back("ffi.Pointer<ffi.Void>");
			}
			for (auto& p : m.params) {
				EmittedType pe = emitForParam(p.type);
				nativeArgs.push_back(pe.ffiNative);
				dartArgs.push_back(pe.ffiDart);
			}

			f << "final " << dartRet << " Function(";
			for (size_t i = 0; i < dartArgs.size(); ++i) {
				if (i) f << ", ";
				f << dartArgs[i];
			}
			f << ") " << sym << " =\n"
			  << "    _dylib.lookupFunction<" << ffiSig(nativeRet, nativeArgs)
			  << ", " << ffiSig(dartRet, dartArgs) << ">('" << sym << "');\n\n";
		}
	}
}

}  // namespace dffi

// =============================================================================
// 7. Dart proxy generation  (one .dart file per class + math.dart)
// =============================================================================

namespace dproxy {

static void writeMathFile(const fs::path& outDir) {
	std::ofstream f(outDir / "math.dart");
	f << "// AUTO-GENERATED by DartApiGenerator.\n";
	f << "// User-facing math types. These are plain Dart classes — the "
		 "proxy\n";
	f << "// layer marshals them across the FFI boundary as POD structs.\n\n";
	f << "import 'dart:ffi' as ffi;\n";
	f << "import 'package:ffi/ffi.dart';\n";
	f << "import 'bindings.dart' as b;\n\n";

	f << "class Vector2 { double x, y; Vector2(this.x, this.y); "
		 "Vector2.zero():x=0,y=0; }\n";
	f << "class Vector3 { double x, y, z; Vector3(this.x, this.y, this.z); "
		 "Vector3.zero():x=0,y=0,z=0; }\n";
	f << "class Vector4 { double x, y, z, w; Vector4(this.x, this.y, this.z, "
		 "this.w); }\n";
	f << "class Quaternion { double x, y, z, w; Quaternion(this.x, this.y, "
		 "this.z, this.w); Quaternion.identity():x=0,y=0,z=0,w=1; }\n";
	f << "class Matrix4 {\n"
	  << "  final List<double> m;\n"
	  << "  Matrix4(this.m) : assert(m.length == 16);\n"
	  << "  Matrix4.identity()\n"
	  << "      : m = List<double>.filled(16, 0.0) {\n"
	  << "    m[0] = m[5] = m[10] = m[15] = 1.0;\n"
	  << "  }\n"
	  << "}\n\n";

	// Marshalling helpers (keep them internal but accessible to proxies).
	f << "// Internal marshalling helpers — not part of the public API.\n";
	f << "b.BVec2 vec2ToC(Vector2 v) { final p = calloc<b.BVec2>(); "
		 "p.ref.x=v.x; p.ref.y=v.y; final r=p.ref; calloc.free(p); return r; "
		 "}\n";
	f << "// For pass-by-value structs we instead allocate temporarily and "
		 "copy.\n";
	f << "// Helper that produces a struct on the stack via a transient "
		 "pointer:\n";
	f << "T withTemp<T extends ffi.Struct, R>(R Function(ffi.Pointer<T>) fn, "
		 "ffi.Pointer<T> Function() alloc, T Function(ffi.Pointer<T>) read, "
		 "void Function(ffi.Pointer<T>) free) { final p=alloc(); try { fn(p); "
		 "return read(p); } finally { free(p); } }\n\n";

	// The simplest working approach: provide pack/unpack that allocates a
	// single BVec*/BMat4 in arena scope. We expose a pair of functions per
	// type.
	f << "ffi.Pointer<b.BVec3> packVec3(ffi.Allocator a, Vector3 v) { final "
		 "p=a<b.BVec3>(); p.ref..x=v.x..y=v.y..z=v.z; return p; }\n";
	f << "Vector3 unpackVec3(b.BVec3 s) => Vector3(s.x, s.y, s.z);\n";
	f << "ffi.Pointer<b.BVec2> packVec2(ffi.Allocator a, Vector2 v) { final "
		 "p=a<b.BVec2>(); p.ref..x=v.x..y=v.y; return p; }\n";
	f << "Vector2 unpackVec2(b.BVec2 s) => Vector2(s.x, s.y);\n";
	f << "ffi.Pointer<b.BVec4> packVec4(ffi.Allocator a, Vector4 v) { final "
		 "p=a<b.BVec4>(); p.ref..x=v.x..y=v.y..z=v.z..w=v.w; return p; }\n";
	f << "Vector4 unpackVec4(b.BVec4 s) => Vector4(s.x, s.y, s.z, s.w);\n";
	f << "ffi.Pointer<b.BQuat> packQuat(ffi.Allocator a, Quaternion v) { final "
		 "p=a<b.BQuat>(); p.ref..x=v.x..y=v.y..z=v.z..w=v.w; return p; }\n";
	f << "Quaternion unpackQuat(b.BQuat s) => Quaternion(s.x, s.y, s.z, "
		 "s.w);\n";
	f << "ffi.Pointer<b.BMat4> packMat4(ffi.Allocator a, Matrix4 v) { final "
		 "p=a<b.BMat4>(); for(var i=0;i<16;i++){ p.ref.m[i]=v.m[i]; } return "
		 "p; }\n";
	f << "Matrix4 unpackMat4(b.BMat4 s) { final l=List<double>.filled(16,0); "
		 "for(var i=0;i<16;i++){ l[i]=s.m[i]; } return Matrix4(l); }\n";
}

// Dart-keyword sanitiser for parameter names.
static std::string safeIdent(std::string s) {
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
		"dynamic",	 "implements", "set"};
	if (kw.count(s)) return s + "_";
	if (s.empty()) return "p";
	return s;
}

// Convert a libclang default-arg token blob into something Dart can accept.
// Returns std::nullopt if we can't make a sensible default.
static std::optional<std::string> dartDefaultFor(const ParamInfo& p) {
	if (p.defaultExpr.empty()) return std::nullopt;
	std::string e = p.defaultExpr;
	// Numeric literals pass through after stripping C++ float suffixes.
	auto isNumeric = [](const std::string& s) {
		if (s.empty()) return false;
		for (char c : s)
			if (!(std::isdigit((unsigned char)c) || c == '.' || c == '-' ||
				  c == '+' || c == 'e' || c == 'E' || c == 'f' || c == 'F'))
				return false;
		return true;
	};
	if (isNumeric(e)) {
		if (!e.empty() && (e.back() == 'f' || e.back() == 'F')) e.pop_back();
		return e;
	}
	if (e == "true" || e == "false") return e;
	if (e == "nullptr" || e == "NULL") return std::string("null");
	return std::nullopt;
}

static std::string dartReturnType(const TypeInfo& t,
								  const ClassInfo* /*self*/) {
	EmittedType e = emitForReturn(t);
	return e.valid ? e.dartUser : "dynamic";
}

static std::string dartParamType(const ParamInfo& p) {
	EmittedType e = emitForParam(p.type);
	return e.valid ? e.dartUser : "dynamic";
}

static void writeClassFile(const fs::path& outDir, const ClassInfo& cls,
						   const std::map<std::string, ClassInfo>& allClasses) {
	std::string snake;
	for (char c : cls.simpleName) {
		if (std::isupper((unsigned char)c) && !snake.empty()) snake += "_";
		snake += (char)std::tolower((unsigned char)c);
	}
	std::ofstream f(outDir / (snake + ".dart"));
	f << "// AUTO-GENERATED by DartApiGenerator. Do not edit by hand.\n";
	f << "// Proxy for " << cls.qualifiedName << ".\n\n";
	f << "import 'dart:ffi' as ffi;\n";
	f << "import 'package:ffi/ffi.dart';\n";
	f << "import 'bindings.dart' as b;\n";
	f << "import 'math.dart';\n";
	// Pull in every other proxy header — simple and avoids cycles since
	// Dart's import graph permits forward references through library scope.
	for (auto& [_, other] : allClasses) {
		if (other.qualifiedName == cls.qualifiedName) continue;
		std::string s;
		for (char c : other.simpleName) {
			if (std::isupper((unsigned char)c) && !s.empty()) s += "_";
			s += (char)std::tolower((unsigned char)c);
		}
		f << "import '" << s << ".dart';\n";
	}
	f << "\n";

	// ----- Class declaration -----
	f << "class " << cls.simpleName << " implements ffi.Finalizable {\n";
	f << "  static final ffi.NativeFinalizer _finalizer =\n"
	  << "      ffi.NativeFinalizer(b." << cls.flatName
	  << "_deletePtr.cast());\n\n";
	f << "  final ffi.Pointer<ffi.Void> handle;\n";
	f << "  bool _disposed = false;\n\n";

	// Internal "wrap existing pointer" constructor — used when a method returns
	// an opaque class pointer that we don't own (we still attach the finalizer
	// because the engine owns the lifetime; if you need non-owning views,
	// generate a separate `.view(handle)` factory).
	f << "  " << cls.simpleName << ".fromHandle(this.handle) {\n"
	  << "    _finalizer.attach(this, handle.cast(), detach: this);\n"
	  << "  }\n\n";

	// Public constructors — emit each ctor as a Dart factory.
	int ctorIdx = 0;
	for (auto& m : cls.methods) {
		if (!m.isConstructor) continue;
		std::string factoryName =
			(ctorIdx == 0)
				? cls.simpleName
				: (cls.simpleName + ".overload" + std::to_string(ctorIdx));

		// Parameter list.
		std::string params;
		for (size_t i = 0; i < m.params.size(); ++i) {
			if (i) params += ", ";
			params +=
				dartParamType(m.params[i]) + " " + safeIdent(m.params[i].name);
			auto dv = dartDefaultFor(m.params[i]);
			if (dv) params += " = " + *dv;
		}

		f << "  factory " << factoryName << "(" << params << ") {\n";
		f << "    return using((arena) {\n";
		// Build call args.
		f << "      final h = b." << cls.flatName << "_new" << m.mangledSuffix
		  << "(";
		for (size_t i = 0; i < m.params.size(); ++i) {
			if (i) f << ", ";
			const auto& p = m.params[i];
			std::string n = safeIdent(p.name);
			switch (p.type.cat) {
				case TypeCat::GlmVec2:
					f << "packVec2(arena, " << n << ").ref";
					break;
				case TypeCat::GlmVec3:
					f << "packVec3(arena, " << n << ").ref";
					break;
				case TypeCat::GlmVec4:
					f << "packVec4(arena, " << n << ").ref";
					break;
				case TypeCat::GlmQuat:
					f << "packQuat(arena, " << n << ").ref";
					break;
				case TypeCat::GlmMat4:
					f << "packMat4(arena, " << n << ")";
					break;
				case TypeCat::OpaqueClass:
					f << n << ".handle";
					break;
				case TypeCat::CString:
					f << n << ".toNativeUtf8(allocator: arena)";
					break;
				case TypeCat::Bool:
					f << n;
					break;
				default:
					f << n;
					break;
			}
		}
		f << ");\n";
		f << "      return " << cls.simpleName << ".fromHandle(h);\n";
		f << "    });\n";
		f << "  }\n\n";
		ctorIdx++;
	}
	if (ctorIdx == 0) {
		// No public ctor — emit a private placeholder so fromHandle remains the
		// only construction path.
		f << "  // (no public C++ constructors visible)\n\n";
	}

	f << "  void dispose() {\n";
	f << "    if (_disposed) return;\n";
	f << "    _finalizer.detach(this);\n";
	f << "    b." << cls.flatName << "_delete(handle);\n";
	f << "    _disposed = true;\n";
	f << "  }\n\n";

	// ----- Methods -----
	for (auto& m : cls.methods) {
		if (m.isConstructor || m.isDestructor) continue;

		EmittedType ret = emitForReturn(m.retType);
		if (!ret.valid) continue;

		// Method name, with overload suffix dropped except when collisions
		// remain.
		std::string dartName = m.name + m.mangledSuffix;

		// Build Dart param list.
		std::string params;
		for (size_t i = 0; i < m.params.size(); ++i) {
			if (i) params += ", ";
			params +=
				dartParamType(m.params[i]) + " " + safeIdent(m.params[i].name);
			auto dv = dartDefaultFor(m.params[i]);
			if (dv) params += " = " + *dv;
		}

		std::string retDart = dartReturnType(m.retType, &cls);
		if (m.retType.cat == TypeCat::GlmMat4) retDart = "Matrix4";

		f << "  " << retDart << " " << dartName << "(" << params << ") {\n";

		// Use an arena to keep transient FFI allocations alive for the call.
		f << "    return using((arena) {\n";

		// Build native call.
		std::string native =
			"b." + cls.flatName + "_" + m.name + m.mangledSuffix;
		std::vector<std::string> args;

		if (ret.isLargeGlm) {
			f << "      final out = arena<b.BMat4>();\n";
			args.push_back("out");
		}
		if (!m.isStatic) args.push_back("handle");

		for (auto& p : m.params) {
			std::string n = safeIdent(p.name);
			switch (p.type.cat) {
				case TypeCat::GlmVec2:
					args.push_back("packVec2(arena, " + n + ").ref");
					break;
				case TypeCat::GlmVec3:
					args.push_back("packVec3(arena, " + n + ").ref");
					break;
				case TypeCat::GlmVec4:
					args.push_back("packVec4(arena, " + n + ").ref");
					break;
				case TypeCat::GlmQuat:
					args.push_back("packQuat(arena, " + n + ").ref");
					break;
				case TypeCat::GlmMat4:
					args.push_back("packMat4(arena, " + n + ")");
					break;
				case TypeCat::OpaqueClass:
					args.push_back(n + ".handle");
					break;
				case TypeCat::CString:
					args.push_back(n + ".toNativeUtf8(allocator: arena)");
					break;
				default:
					args.push_back(n);
			}
		}

		std::string callExpr = native + "(";
		for (size_t i = 0; i < args.size(); ++i) {
			if (i) callExpr += ", ";
			callExpr += args[i];
		}
		callExpr += ")";

		switch (m.retType.cat) {
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
			case TypeCat::GlmMat4:
				f << "      " << callExpr << ";\n";
				f << "      return unpackMat4(out.ref);\n";
				break;
			case TypeCat::OpaqueClass: {
				std::string cls2 = lastIdent(m.retType.cppCanonical);
				f << "      final p = " << callExpr << ";\n";
				f << "      if (p == ffi.nullptr) throw StateError('null "
					 "returned from "
				  << dartName << "');\n";
				f << "      return " << cls2 << ".fromHandle(p);\n";
				break;
			}
			case TypeCat::CString:
				f << "      final p = " << callExpr << ";\n";
				f << "      return p == ffi.nullptr ? '' : p.toDartString();\n";
				break;
			default:
				f << "      return " << callExpr << ";\n";
		}
		f << "    });\n";
		f << "  }\n\n";
	}

	f << "}\n";
}

static void writeFiles(const fs::path& outDir,
					   const std::map<std::string, ClassInfo>& classes) {
	writeMathFile(outDir);
	for (auto& [_, cls] : classes) writeClassFile(outDir, cls, classes);
}

}  // namespace dproxy

// =============================================================================
// 8. Driver
// =============================================================================

struct CliArgs {
	fs::path outDir;
	std::string libName = "libbird_engine";
	std::vector<fs::path> headers;
	std::vector<std::string> clangArgs;
};

static void printUsage() {
	std::cerr << "Usage: DartApiGenerator --out <dir> [--lib <name>] "
				 "[<clang flags...> --] <header>...\n"
				 "\n"
				 "Required:\n"
				 "  --out <dir>   Output directory for generated files.\n"
				 "Optional:\n"
				 "  --lib <name>  Shared-library base name Dart will load "
				 "(default: libbird_engine).\n"
				 "  -- ...        Everything after '--' is forwarded to "
				 "libclang as-is.\n";
}

static bool parseArgs(int argc, char** argv, CliArgs& out) {
	bool sawDoubleDash = false;
	for (int i = 1; i < argc; ++i) {
		std::string a = argv[i];
		if (sawDoubleDash) {
			// After the first --, the next non-flag is a header; flags continue
			// forwarding to libclang. We simplify: treat all remaining as
			// either headers (if file exists) or clang args.
			fs::path p(a);
			if (fs::exists(p))
				out.headers.push_back(p);
			else
				out.clangArgs.push_back(a);
			continue;
		}
		if (a == "--out" && i + 1 < argc)
			out.outDir = argv[++i];
		else if (a == "--lib" && i + 1 < argc)
			out.libName = argv[++i];
		else if (a == "--")
			sawDoubleDash = true;
		else if (a == "-h" || a == "--help") {
			printUsage();
			std::exit(0);
		} else {
			// Unknown leading flag → treat as header if it exists, else clang
			// flag.
			fs::path p(a);
			if (fs::exists(p) && !fs::is_directory(p))
				out.headers.push_back(p);
			else
				out.clangArgs.push_back(a);
		}
	}
	if (out.outDir.empty() || out.headers.empty()) {
		printUsage();
		return false;
	}
	return true;
}

int main(int argc, char** argv) {
	CliArgs cli;
	if (!parseArgs(argc, argv, cli)) return 1;

	fs::create_directories(cli.outDir);

	CXIndex index = clang_createIndex(/*excludeDeclarationsFromPCH=*/0,
									  /*displayDiagnostics=*/1);

	// Build a single argv array for libclang.
	std::vector<const char*> args;
	args.push_back("-x");
	args.push_back("c++");
	args.push_back("-std=c++17");
	for (auto& a : cli.clangArgs) args.push_back(a.c_str());

	VisitorState state;
	std::set<std::string> nsFilter = {"bird"};
	state.nsFilter = &nsFilter;

	for (auto& header : cli.headers) {
		CXTranslationUnit tu = nullptr;
		CXErrorCode ec = clang_parseTranslationUnit2(
			index, header.string().c_str(), args.data(),
			static_cast<int>(args.size()), nullptr, 0,
			CXTranslationUnit_DetailedPreprocessingRecord |
				CXTranslationUnit_SkipFunctionBodies,
			&tu);
		if (ec != CXError_Success || !tu) {
			std::cerr << "[error] failed to parse " << header << "\n";
			continue;
		}

		// Surface compiler diagnostics so missing -I flags don't silently
		// produce empty output.
		unsigned nDiag = clang_getNumDiagnostics(tu);
		for (unsigned i = 0; i < nDiag; ++i) {
			CXDiagnostic d = clang_getDiagnostic(tu, i);
			CXDiagnosticSeverity sev = clang_getDiagnosticSeverity(d);
			if (sev >= CXDiagnostic_Error) {
				std::cerr << "[clang] " << toStr(clang_formatDiagnostic(d, 0))
						  << "\n";
			}
			clang_disposeDiagnostic(d);
		}

		state.tuFile = header;
		clang_visitChildren(clang_getTranslationUnitCursor(tu), visitTU,
							&state);
		clang_disposeTranslationUnit(tu);
	}

	clang_disposeIndex(index);

	if (state.classes.empty()) {
		std::cerr << "[warn] no exportable classes found.\n";
		return 0;
	}

	// Pre-process: disambiguate overloads per class.
	for (auto& [_, cls] : state.classes) disambiguateOverloads(cls);

	// ----- Emit the three tiers -----
	cwrap::writeFiles(cli.outDir, cli.headers, state.classes);
	dffi ::writeFile(cli.outDir, cli.libName, state.classes);
	dproxy::writeFiles(cli.outDir, state.classes);

	std::cout << "Generated " << state.classes.size() << " class(es) into "
			  << cli.outDir << "\n";
	return 0;
}
