import 'dart:ffi' as ffi;

import 'generated/bindings.dart' as b;
import 'generated/component.dart';
import 'generated/globals.dart';
import 'generated/input.dart';
import 'generated/resource_manager.dart';
import 'generated/scene.dart';
import 'generated/ui_object.dart';
import 'generated/world_object.dart';

typedef _NativeInitSig = ffi.Void Function(ffi.Pointer<ffi.Void>);
typedef _NativeProcessSig = ffi.Void Function(
    ffi.Pointer<ffi.Void>, ffi.Float);
typedef _NativeDeinitSig = ffi.Void Function(ffi.Pointer<ffi.Void>);

typedef _CreateDartScriptNative = ffi.Pointer<ffi.Void> Function(
  ffi.Pointer<ffi.NativeFunction<_NativeInitSig>>,
  ffi.Pointer<ffi.NativeFunction<_NativeProcessSig>>,
  ffi.Pointer<ffi.NativeFunction<_NativeProcessSig>>,
  ffi.Pointer<ffi.NativeFunction<_NativeDeinitSig>>,
);
typedef _CreateDartScriptDart = ffi.Pointer<ffi.Void> Function(
  ffi.Pointer<ffi.NativeFunction<_NativeInitSig>>,
  ffi.Pointer<ffi.NativeFunction<_NativeProcessSig>>,
  ffi.Pointer<ffi.NativeFunction<_NativeProcessSig>>,
  ffi.Pointer<ffi.NativeFunction<_NativeDeinitSig>>,
);

late final _CreateDartScriptDart _createDartScriptComponent = b.nativeLib
    .lookupFunction<_CreateDartScriptNative, _CreateDartScriptDart>(
        'bird_DartScriptComponent_new');

typedef _EntityGetIdNative = ffi.Uint64 Function(ffi.Pointer<ffi.Void>);
typedef _EntityGetIdDart = int Function(ffi.Pointer<ffi.Void>);
typedef _EntityResolveNative = ffi.Pointer<ffi.Void> Function(ffi.Uint64);
typedef _EntityResolveDart = ffi.Pointer<ffi.Void> Function(int);
typedef _EntityIsValidNative = ffi.Bool Function(ffi.Uint64);
typedef _EntityIsValidDart = bool Function(int);

late final _EntityGetIdDart _entityRegistryGetId = b.nativeLib
    .lookupFunction<_EntityGetIdNative, _EntityGetIdDart>(
        'bird_EntityRegistry_getID');
late final _EntityResolveDart _entityRegistryResolve = b.nativeLib
    .lookupFunction<_EntityResolveNative, _EntityResolveDart>(
        'bird_EntityRegistry_resolve');
late final _EntityIsValidDart _entityRegistryIsValid = b.nativeLib
    .lookupFunction<_EntityIsValidNative, _EntityIsValidDart>(
        'bird_EntityRegistry_isValid');

/// Hot-reload-safe entity handle backed by a stable native `EntityID`
/// (`WorldObject::getID()`).
///
/// Prefer storing [EntityHandle] instead of raw [WorldObject] proxies in
/// long-lived Dart script state so Dart hot-reloads and native entity
/// destruction never leave dangling pointers.
class EntityHandle {
  /// Stable, monotonically increasing native entity ID (0 is invalid).
  final int id;

  const EntityHandle(this.id);

  /// Creates an [EntityHandle] from a live native [WorldObject] pointer.
  factory EntityHandle.fromPointer(ffi.Pointer<ffi.Void> ptr) =>
      EntityHandle(_entityRegistryGetId(ptr));

  /// Returns true if the native entity is still alive in the engine.
  bool get isValid => id != 0 && _entityRegistryIsValid(id);

  /// Resolves this handle to a borrowed [WorldObject] proxy, or `null` if the
  /// native entity has been destroyed.
  WorldObject? resolve() {
    if (id == 0) return null;
    final ptr = _entityRegistryResolve(id);
    if (ptr == ffi.nullptr) return null;
    return WorldObject.fromHandle(ptr, owned: false);
  }
}

/// Base class for Dart scripts attached to a [WorldObject] (or [Scene]),
/// mirroring C++ `bird::ProcessScript<T>`.
///
/// Subclass [BirdScript], override [init], [process], [physicsProcess], and/or
/// [deinit], and attach it to an entity via `entity.addComponent(myScript.createComponent())`.
abstract class BirdScript<T extends WorldObject> {
  WorldObject? _host;
  ffi.NativeCallable<_NativeInitSig>? _initCb;
  ffi.NativeCallable<_NativeProcessSig>? _processCb;
  ffi.NativeCallable<_NativeProcessSig>? _physicsProcessCb;
  ffi.NativeCallable<_NativeDeinitSig>? _deinitCb;

  /// Wraps the raw native host pointer into the typed entity [T].
  /// Override if using a subclass of [WorldObject] (e.g. `Camera.fromHandle`).
  T wrapHost(ffi.Pointer<ffi.Void> ptr) =>
      WorldObject.fromHandle(ptr, owned: false) as T;

  /// The host entity this script is attached to.
  T get object => _host! as T;

  /// The parent [Scene] of the host entity, if attached.
  Scene? get scene => _host?.getParent();

  /// The global [Input] singleton.
  Input? get inputManager => input;

  /// The global [ResourceManager] singleton.
  ResourceManager? get resources => resourceManager;

  /// Called once when attached to an initialized entity or when the scene initializes.
  void init() {}

  /// Called every frame before rendering with elapsed frame time [delta] in seconds.
  void process(double delta) {}

  /// Called at a fixed timestep before each physics world step.
  void physicsProcess(double fixedDelta) {}

  /// Called when the component is removed or its host entity is deinitialized.
  void deinit() {}

  /// Creates the native `Component` bridge backed by this Dart script instance.
  ///
  /// Pass the returned [Component] directly to `worldObject.addComponent(...)`.
  Component createComponent() {
    _initCb = ffi.NativeCallable<_NativeInitSig>.isolateLocal(
      (ffi.Pointer<ffi.Void> hostPtr) {
        _host = wrapHost(hostPtr);
        init();
      },
    );
    _processCb = ffi.NativeCallable<_NativeProcessSig>.isolateLocal(
      (ffi.Pointer<ffi.Void> hostPtr, double delta) {
        _host ??= wrapHost(hostPtr);
        process(delta);
      },
    );
    _physicsProcessCb = ffi.NativeCallable<_NativeProcessSig>.isolateLocal(
      (ffi.Pointer<ffi.Void> hostPtr, double fixedDelta) {
        _host ??= wrapHost(hostPtr);
        physicsProcess(fixedDelta);
      },
    );
    _deinitCb = ffi.NativeCallable<_NativeDeinitSig>.isolateLocal(
      (ffi.Pointer<ffi.Void> hostPtr) {
        try {
          deinit();
        } finally {
          _initCb?.close();
          _processCb?.close();
          _physicsProcessCb?.close();
          _deinitCb?.close();
          _initCb = null;
          _processCb = null;
          _physicsProcessCb = null;
          _deinitCb = null;
        }
      },
    );

    final ptr = _createDartScriptComponent(
      _initCb!.nativeFunction,
      _processCb!.nativeFunction,
      _physicsProcessCb!.nativeFunction,
      _deinitCb!.nativeFunction,
    );
    return Component.fromHandle(ptr, owned: true);
  }
}

/// Converts a raw scene pointer address passed to `appEntry(int sceneAddr)`
/// into a borrowed [Scene] proxy.
Scene sceneFromAddress(int sceneAddr) =>
    Scene.fromHandle(ffi.Pointer<ffi.Void>.fromAddress(sceneAddr), owned: false);

/// Fluent helper mirroring C++ `UIObject::add<T>(...)` for building UI trees.
extension UIObjectAddExtension on UIObject {
  /// Appends [child] to this node (transferring ownership to C++) and returns it.
  T add<T extends UIObject>(T child) {
    addChild(child);
    return child;
  }
}
