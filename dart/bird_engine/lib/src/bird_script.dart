import 'dart:async';
import 'dart:ffi' as ffi;
import 'package:ffi/ffi.dart' as pkg_ffi;

import 'events.dart';
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

typedef _SetScriptMetadataNative = ffi.Void Function(
    ffi.Pointer<ffi.Void>, ffi.Pointer<pkg_ffi.Utf8>, ffi.Pointer<pkg_ffi.Utf8>);
typedef _SetScriptMetadataDart = void Function(
    ffi.Pointer<ffi.Void>, ffi.Pointer<pkg_ffi.Utf8>, ffi.Pointer<pkg_ffi.Utf8>);

typedef _SetPropBoolNative = ffi.Void Function(
    ffi.Pointer<ffi.Void>, ffi.Pointer<pkg_ffi.Utf8>, ffi.Bool);
typedef _SetPropBoolDart = void Function(
    ffi.Pointer<ffi.Void>, ffi.Pointer<pkg_ffi.Utf8>, bool);

typedef _SetPropIntNative = ffi.Void Function(
    ffi.Pointer<ffi.Void>, ffi.Pointer<pkg_ffi.Utf8>, ffi.Int64);
typedef _SetPropIntDart = void Function(
    ffi.Pointer<ffi.Void>, ffi.Pointer<pkg_ffi.Utf8>, int);

typedef _SetPropDoubleNative = ffi.Void Function(
    ffi.Pointer<ffi.Void>, ffi.Pointer<pkg_ffi.Utf8>, ffi.Double);
typedef _SetPropDoubleDart = void Function(
    ffi.Pointer<ffi.Void>, ffi.Pointer<pkg_ffi.Utf8>, double);

typedef _SetPropStringNative = ffi.Void Function(
    ffi.Pointer<ffi.Void>, ffi.Pointer<pkg_ffi.Utf8>, ffi.Pointer<pkg_ffi.Utf8>);
typedef _SetPropStringDart = void Function(
    ffi.Pointer<ffi.Void>, ffi.Pointer<pkg_ffi.Utf8>, ffi.Pointer<pkg_ffi.Utf8>);

late final _SetScriptMetadataDart _setScriptMetadata = b.nativeLib
    .lookupFunction<_SetScriptMetadataNative, _SetScriptMetadataDart>(
        'bird_DartScriptComponent_setMetadata');
late final _SetPropBoolDart _setPropBool = b.nativeLib
    .lookupFunction<_SetPropBoolNative, _SetPropBoolDart>(
        'bird_DartScriptComponent_setPropertyBool');
late final _SetPropIntDart _setPropInt = b.nativeLib
    .lookupFunction<_SetPropIntNative, _SetPropIntDart>(
        'bird_DartScriptComponent_setPropertyInt');
late final _SetPropDoubleDart _setPropDouble = b.nativeLib
    .lookupFunction<_SetPropDoubleNative, _SetPropDoubleDart>(
        'bird_DartScriptComponent_setPropertyDouble');
late final _SetPropStringDart _setPropString = b.nativeLib
    .lookupFunction<_SetPropStringNative, _SetPropStringDart>(
        'bird_DartScriptComponent_setPropertyString');

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

  /// Serialized class name for scene/prefab persistence. Defaults to
  /// [runtimeType.toString()].
  String get scriptClassName => runtimeType.toString();

  /// Optional Dart source or package path recorded during scene serialization.
  String get scriptPath => '';

  /// Exposed serializable properties (`bool`, `int`, `double`, `String`)
  /// synchronized onto the native `DartScriptComponent` when attached.
  Map<String, Object?> get exposedProperties => const {};

  /// Creates the native `Component` bridge backed by this Dart script instance.
  ///
  /// Pass the returned [Component] directly to `worldObject.addComponent(...)`.
  Component createComponent() {
    _initCb = ffi.NativeCallable<_NativeInitSig>.isolateLocal(
      (ffi.Pointer<ffi.Void> hostPtr) {
        _host = wrapHost(hostPtr);
        runZoned(
          () => init(),
          zoneValues: <Object?, Object?>{
            birdActiveEntityIdZoneKey: _host!.getID(),
          },
        );
      },
    );
    _processCb = ffi.NativeCallable<_NativeProcessSig>.isolateLocal(
      (ffi.Pointer<ffi.Void> hostPtr, double delta) {
        _host ??= wrapHost(hostPtr);
        runZoned(
          () => process(delta),
          zoneValues: <Object?, Object?>{
            birdActiveEntityIdZoneKey: _host!.getID(),
          },
        );
      },
    );
    _physicsProcessCb = ffi.NativeCallable<_NativeProcessSig>.isolateLocal(
      (ffi.Pointer<ffi.Void> hostPtr, double fixedDelta) {
        _host ??= wrapHost(hostPtr);
        runZoned(
          () => physicsProcess(fixedDelta),
          zoneValues: <Object?, Object?>{
            birdActiveEntityIdZoneKey: _host!.getID(),
          },
        );
      },
    );
    _deinitCb = ffi.NativeCallable<_NativeDeinitSig>.isolateLocal(
      (ffi.Pointer<ffi.Void> hostPtr) {
        _host ??= wrapHost(hostPtr);
        try {
          runZoned(
            () => deinit(),
            zoneValues: <Object?, Object?>{
              birdActiveEntityIdZoneKey: _host!.getID(),
            },
          );
        } finally {
          if (_host != null) {
            Events.unsubscribeByOwner(ownerEntityId: _host!.getID());
          }
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

    final classPtr = scriptClassName.toNativeUtf8();
    final pathPtr = scriptPath.toNativeUtf8();
    try {
      _setScriptMetadata(ptr, classPtr, pathPtr);
    } finally {
      pkg_ffi.calloc.free(classPtr);
      pkg_ffi.calloc.free(pathPtr);
    }

    for (final entry in exposedProperties.entries) {
      final namePtr = entry.key.toNativeUtf8();
      try {
        final val = entry.value;
        if (val is bool) {
          _setPropBool(ptr, namePtr, val);
        } else if (val is int) {
          _setPropInt(ptr, namePtr, val);
        } else if (val is double) {
          _setPropDouble(ptr, namePtr, val);
        } else if (val is String) {
          final valPtr = val.toNativeUtf8();
          try {
            _setPropString(ptr, namePtr, valPtr);
          } finally {
            pkg_ffi.calloc.free(valPtr);
          }
        }
      } finally {
        pkg_ffi.calloc.free(namePtr);
      }
    }

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
