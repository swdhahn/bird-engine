import 'dart:ffi' as ffi;

import 'package:ffi/ffi.dart';

import 'bird_diagnostics.dart';
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

typedef _DisableDartScriptNative = ffi.Void Function(
  ffi.Pointer<ffi.Void> componentPtr,
  ffi.Pointer<Utf8> reason,
);
typedef _DisableDartScriptDart = void Function(
  ffi.Pointer<ffi.Void> componentPtr,
  ffi.Pointer<Utf8> reason,
);

late final _DisableDartScriptDart _disableDartScriptComponent = b.nativeLib
    .lookupFunction<_DisableDartScriptNative, _DisableDartScriptDart>(
        'bird_DartScriptComponent_disable');

/// Base class for Dart scripts attached to a [WorldObject] (or [Scene]),
/// mirroring C++ `bird::ProcessScript<T>`.
///
/// Subclass [BirdScript], override [init], [process], [physicsProcess], and/or
/// [deinit], and attach it to an entity via `entity.addComponent(myScript.createComponent())`.
abstract class BirdScript<T extends WorldObject> {
  WorldObject? _host;
  ffi.Pointer<ffi.Void>? _nativeComponentPtr;
  bool _disabled = false;
  ffi.NativeCallable<_NativeInitSig>? _initCb;
  ffi.NativeCallable<_NativeProcessSig>? _processCb;
  ffi.NativeCallable<_NativeProcessSig>? _physicsProcessCb;
  ffi.NativeCallable<_NativeDeinitSig>? _deinitCb;

  /// True if this script component was disabled after throwing an unhandled
  /// exception in [init], [process], [physicsProcess], or [deinit].
  bool get isDisabled => _disabled;

  void _handleScriptError(String phase, Object error, StackTrace stackTrace) {
    final summary = '$runtimeType.$phase() threw: $error';
    BirdLog.error('DartScript', '$summary\n$stackTrace');
    _disabled = true;
    final ptr = _nativeComponentPtr;
    if (ptr != null && ptr != ffi.nullptr) {
      final reasonPtr = summary.toNativeUtf8();
      try {
        _disableDartScriptComponent(ptr, reasonPtr);
      } finally {
        calloc.free(reasonPtr);
      }
    }
  }

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
    _disabled = false;
    _initCb = ffi.NativeCallable<_NativeInitSig>.isolateLocal(
      (ffi.Pointer<ffi.Void> hostPtr) {
        if (_disabled) return;
        try {
          _host = wrapHost(hostPtr);
          init();
        } catch (error, stackTrace) {
          _handleScriptError('init', error, stackTrace);
        }
      },
    );
    _processCb = ffi.NativeCallable<_NativeProcessSig>.isolateLocal(
      (ffi.Pointer<ffi.Void> hostPtr, double delta) {
        if (_disabled) return;
        try {
          _host ??= wrapHost(hostPtr);
          process(delta);
        } catch (error, stackTrace) {
          _handleScriptError('process', error, stackTrace);
        }
      },
    );
    _physicsProcessCb = ffi.NativeCallable<_NativeProcessSig>.isolateLocal(
      (ffi.Pointer<ffi.Void> hostPtr, double fixedDelta) {
        if (_disabled) return;
        try {
          _host ??= wrapHost(hostPtr);
          physicsProcess(fixedDelta);
        } catch (error, stackTrace) {
          _handleScriptError('physicsProcess', error, stackTrace);
        }
      },
    );
    _deinitCb = ffi.NativeCallable<_NativeDeinitSig>.isolateLocal(
      (ffi.Pointer<ffi.Void> hostPtr) {
        try {
          if (!_disabled) {
            deinit();
          }
        } catch (error, stackTrace) {
          _handleScriptError('deinit', error, stackTrace);
        } finally {
          _nativeComponentPtr = null;
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
    _nativeComponentPtr = ptr;
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
