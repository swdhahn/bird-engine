import 'dart:async';
import 'dart:ffi' as ffi;
import 'package:ffi/ffi.dart';

import 'generated/bindings.dart' as b;
import 'generated/event_payload.dart';
import 'generated/math.dart';
import 'generated/world_object.dart';

// ---------------------------------------------------------------------------
// Native FFI Signatures for DartEventBridge
// ---------------------------------------------------------------------------

typedef _NativeDispatchSig = ffi.Void Function(
    ffi.Uint64 subscriptionId, ffi.Pointer<ffi.Void> payloadPtr);
typedef _NativeUnsubscribeSig = ffi.Void Function(ffi.Uint64 subscriptionId);

typedef _RegisterCallbacksNative = ffi.Void Function(
  ffi.Pointer<ffi.NativeFunction<_NativeDispatchSig>>,
  ffi.Pointer<ffi.NativeFunction<_NativeUnsubscribeSig>>,
);
typedef _RegisterCallbacksDart = void Function(
  ffi.Pointer<ffi.NativeFunction<_NativeDispatchSig>>,
  ffi.Pointer<ffi.NativeFunction<_NativeUnsubscribeSig>>,
);

typedef _UnregisterCallbacksNative = ffi.Void Function();
typedef _UnregisterCallbacksDart = void Function();

typedef _PayloadNewNative = ffi.Pointer<ffi.Void> Function();
typedef _PayloadNewDart = ffi.Pointer<ffi.Void> Function();

typedef _PayloadDeleteNative = ffi.Void Function(ffi.Pointer<ffi.Void>);
typedef _PayloadDeleteDart = void Function(ffi.Pointer<ffi.Void>);

typedef _PayloadSetNullNative = ffi.Void Function(
    ffi.Pointer<ffi.Void>, ffi.Pointer<Utf8>);
typedef _PayloadSetNullDart = void Function(
    ffi.Pointer<ffi.Void>, ffi.Pointer<Utf8>);

typedef _PayloadSetBoolNative = ffi.Void Function(
    ffi.Pointer<ffi.Void>, ffi.Pointer<Utf8>, ffi.Bool);
typedef _PayloadSetBoolDart = void Function(
    ffi.Pointer<ffi.Void>, ffi.Pointer<Utf8>, bool);

typedef _PayloadSetIntNative = ffi.Void Function(
    ffi.Pointer<ffi.Void>, ffi.Pointer<Utf8>, ffi.Int64);
typedef _PayloadSetIntDart = void Function(
    ffi.Pointer<ffi.Void>, ffi.Pointer<Utf8>, int);

typedef _PayloadSetDoubleNative = ffi.Void Function(
    ffi.Pointer<ffi.Void>, ffi.Pointer<Utf8>, ffi.Double);
typedef _PayloadSetDoubleDart = void Function(
    ffi.Pointer<ffi.Void>, ffi.Pointer<Utf8>, double);

typedef _PayloadSetStringNative = ffi.Void Function(
    ffi.Pointer<ffi.Void>, ffi.Pointer<Utf8>, ffi.Pointer<Utf8>);
typedef _PayloadSetStringDart = void Function(
    ffi.Pointer<ffi.Void>, ffi.Pointer<Utf8>, ffi.Pointer<Utf8>);

typedef _PayloadSetVec2Native = ffi.Void Function(
    ffi.Pointer<ffi.Void>, ffi.Pointer<Utf8>, ffi.Float, ffi.Float);
typedef _PayloadSetVec2Dart = void Function(
    ffi.Pointer<ffi.Void>, ffi.Pointer<Utf8>, double, double);

typedef _PayloadSetVec3Native = ffi.Void Function(
    ffi.Pointer<ffi.Void>, ffi.Pointer<Utf8>, ffi.Float, ffi.Float, ffi.Float);
typedef _PayloadSetVec3Dart = void Function(
    ffi.Pointer<ffi.Void>, ffi.Pointer<Utf8>, double, double, double);

typedef _PayloadSetVec4Native = ffi.Void Function(ffi.Pointer<ffi.Void>,
    ffi.Pointer<Utf8>, ffi.Float, ffi.Float, ffi.Float, ffi.Float);
typedef _PayloadSetVec4Dart = void Function(ffi.Pointer<ffi.Void>,
    ffi.Pointer<Utf8>, double, double, double, double);

typedef _PayloadFieldCountNative = ffi.Uint32 Function(ffi.Pointer<ffi.Void>);
typedef _PayloadFieldCountDart = int Function(ffi.Pointer<ffi.Void>);

typedef _PayloadFieldKeyNative = ffi.Pointer<Utf8> Function(
    ffi.Pointer<ffi.Void>, ffi.Uint32);
typedef _PayloadFieldKeyDart = ffi.Pointer<Utf8> Function(
    ffi.Pointer<ffi.Void>, int);

typedef _PayloadFieldTypeNative = ffi.Uint8 Function(
    ffi.Pointer<ffi.Void>, ffi.Uint32);
typedef _PayloadFieldTypeDart = int Function(ffi.Pointer<ffi.Void>, int);

typedef _PayloadFieldBoolNative = ffi.Bool Function(
    ffi.Pointer<ffi.Void>, ffi.Uint32);
typedef _PayloadFieldBoolDart = bool Function(ffi.Pointer<ffi.Void>, int);

typedef _PayloadFieldIntNative = ffi.Int64 Function(
    ffi.Pointer<ffi.Void>, ffi.Uint32);
typedef _PayloadFieldIntDart = int Function(ffi.Pointer<ffi.Void>, int);

typedef _PayloadFieldDoubleNative = ffi.Double Function(
    ffi.Pointer<ffi.Void>, ffi.Uint32);
typedef _PayloadFieldDoubleDart = double Function(ffi.Pointer<ffi.Void>, int);

typedef _PayloadFieldStringNative = ffi.Pointer<Utf8> Function(
    ffi.Pointer<ffi.Void>, ffi.Uint32);
typedef _PayloadFieldStringDart = ffi.Pointer<Utf8> Function(
    ffi.Pointer<ffi.Void>, int);

typedef _PayloadFieldVec2Native = ffi.Void Function(ffi.Pointer<ffi.Void>,
    ffi.Uint32, ffi.Pointer<ffi.Float>, ffi.Pointer<ffi.Float>);
typedef _PayloadFieldVec2Dart = void Function(ffi.Pointer<ffi.Void>, int,
    ffi.Pointer<ffi.Float>, ffi.Pointer<ffi.Float>);

typedef _PayloadFieldVec3Native = ffi.Void Function(
    ffi.Pointer<ffi.Void>,
    ffi.Uint32,
    ffi.Pointer<ffi.Float>,
    ffi.Pointer<ffi.Float>,
    ffi.Pointer<ffi.Float>);
typedef _PayloadFieldVec3Dart = void Function(
    ffi.Pointer<ffi.Void>,
    int,
    ffi.Pointer<ffi.Float>,
    ffi.Pointer<ffi.Float>,
    ffi.Pointer<ffi.Float>);

typedef _PayloadFieldVec4Native = ffi.Void Function(
    ffi.Pointer<ffi.Void>,
    ffi.Uint32,
    ffi.Pointer<ffi.Float>,
    ffi.Pointer<ffi.Float>,
    ffi.Pointer<ffi.Float>,
    ffi.Pointer<ffi.Float>);
typedef _PayloadFieldVec4Dart = void Function(
    ffi.Pointer<ffi.Void>,
    int,
    ffi.Pointer<ffi.Float>,
    ffi.Pointer<ffi.Float>,
    ffi.Pointer<ffi.Float>,
    ffi.Pointer<ffi.Float>);

typedef _EmitNative = ffi.Bool Function(
    ffi.Pointer<Utf8>, ffi.Pointer<ffi.Void>);
typedef _EmitDart = bool Function(ffi.Pointer<Utf8>, ffi.Pointer<ffi.Void>);

typedef _EmitImmediateNative = ffi.Void Function(
    ffi.Pointer<Utf8>, ffi.Pointer<ffi.Void>);
typedef _EmitImmediateDart = void Function(
    ffi.Pointer<Utf8>, ffi.Pointer<ffi.Void>);

typedef _SubscribeNative = ffi.Uint64 Function(ffi.Pointer<Utf8>, ffi.Uint64);
typedef _SubscribeDart = int Function(ffi.Pointer<Utf8>, int);

typedef _UnsubscribeNative = ffi.Bool Function(ffi.Uint64);
typedef _UnsubscribeDart = bool Function(int);

typedef _UnsubscribeByOwnerNative = ffi.Uint64 Function(ffi.Uint64);
typedef _UnsubscribeByOwnerDart = int Function(int);

typedef _DispatchQueuedNative = ffi.Uint64 Function();
typedef _DispatchQueuedDart = int Function();

// ---------------------------------------------------------------------------
// Lazy FFI Symbol Lookups
// ---------------------------------------------------------------------------

late final _RegisterCallbacksDart _registerCallbacks = b.nativeLib
    .lookupFunction<_RegisterCallbacksNative, _RegisterCallbacksDart>(
        'bird_events_register_callbacks');

late final _UnregisterCallbacksDart _unregisterCallbacks = b.nativeLib
    .lookupFunction<_UnregisterCallbacksNative, _UnregisterCallbacksDart>(
        'bird_events_unregister_callbacks');

late final _PayloadNewDart _payloadNew = b.nativeLib
    .lookupFunction<_PayloadNewNative, _PayloadNewDart>(
        'bird_events_payload_new');

late final _PayloadDeleteDart _payloadDelete = b.nativeLib
    .lookupFunction<_PayloadDeleteNative, _PayloadDeleteDart>(
        'bird_events_payload_delete');

late final _PayloadSetNullDart _payloadSetNull = b.nativeLib
    .lookupFunction<_PayloadSetNullNative, _PayloadSetNullDart>(
        'bird_events_payload_set_null');

late final _PayloadSetBoolDart _payloadSetBool = b.nativeLib
    .lookupFunction<_PayloadSetBoolNative, _PayloadSetBoolDart>(
        'bird_events_payload_set_bool');

late final _PayloadSetIntDart _payloadSetInt = b.nativeLib
    .lookupFunction<_PayloadSetIntNative, _PayloadSetIntDart>(
        'bird_events_payload_set_int');

late final _PayloadSetDoubleDart _payloadSetDouble = b.nativeLib
    .lookupFunction<_PayloadSetDoubleNative, _PayloadSetDoubleDart>(
        'bird_events_payload_set_double');

late final _PayloadSetStringDart _payloadSetString = b.nativeLib
    .lookupFunction<_PayloadSetStringNative, _PayloadSetStringDart>(
        'bird_events_payload_set_string');

late final _PayloadSetVec2Dart _payloadSetVec2 = b.nativeLib
    .lookupFunction<_PayloadSetVec2Native, _PayloadSetVec2Dart>(
        'bird_events_payload_set_vec2');

late final _PayloadSetVec3Dart _payloadSetVec3 = b.nativeLib
    .lookupFunction<_PayloadSetVec3Native, _PayloadSetVec3Dart>(
        'bird_events_payload_set_vec3');

late final _PayloadSetVec4Dart _payloadSetVec4 = b.nativeLib
    .lookupFunction<_PayloadSetVec4Native, _PayloadSetVec4Dart>(
        'bird_events_payload_set_vec4');

late final _PayloadFieldCountDart _payloadFieldCount = b.nativeLib
    .lookupFunction<_PayloadFieldCountNative, _PayloadFieldCountDart>(
        'bird_events_payload_field_count');

late final _PayloadFieldKeyDart _payloadFieldKey = b.nativeLib
    .lookupFunction<_PayloadFieldKeyNative, _PayloadFieldKeyDart>(
        'bird_events_payload_field_key');

late final _PayloadFieldTypeDart _payloadFieldType = b.nativeLib
    .lookupFunction<_PayloadFieldTypeNative, _PayloadFieldTypeDart>(
        'bird_events_payload_field_type');

late final _PayloadFieldBoolDart _payloadFieldBool = b.nativeLib
    .lookupFunction<_PayloadFieldBoolNative, _PayloadFieldBoolDart>(
        'bird_events_payload_field_bool');

late final _PayloadFieldIntDart _payloadFieldInt = b.nativeLib
    .lookupFunction<_PayloadFieldIntNative, _PayloadFieldIntDart>(
        'bird_events_payload_field_int');

late final _PayloadFieldDoubleDart _payloadFieldDouble = b.nativeLib
    .lookupFunction<_PayloadFieldDoubleNative, _PayloadFieldDoubleDart>(
        'bird_events_payload_field_double');

late final _PayloadFieldStringDart _payloadFieldString = b.nativeLib
    .lookupFunction<_PayloadFieldStringNative, _PayloadFieldStringDart>(
        'bird_events_payload_field_string');

late final _PayloadFieldVec2Dart _payloadFieldVec2 = b.nativeLib
    .lookupFunction<_PayloadFieldVec2Native, _PayloadFieldVec2Dart>(
        'bird_events_payload_field_vec2');

late final _PayloadFieldVec3Dart _payloadFieldVec3 = b.nativeLib
    .lookupFunction<_PayloadFieldVec3Native, _PayloadFieldVec3Dart>(
        'bird_events_payload_field_vec3');

late final _PayloadFieldVec4Dart _payloadFieldVec4 = b.nativeLib
    .lookupFunction<_PayloadFieldVec4Native, _PayloadFieldVec4Dart>(
        'bird_events_payload_field_vec4');

late final _EmitDart _emit =
    b.nativeLib.lookupFunction<_EmitNative, _EmitDart>('bird_events_emit');

late final _EmitImmediateDart _emitImmediate = b.nativeLib
    .lookupFunction<_EmitImmediateNative, _EmitImmediateDart>(
        'bird_events_emit_immediate');

late final _SubscribeDart _subscribe = b.nativeLib
    .lookupFunction<_SubscribeNative, _SubscribeDart>('bird_events_subscribe');

late final _UnsubscribeDart _unsubscribe = b.nativeLib
    .lookupFunction<_UnsubscribeNative, _UnsubscribeDart>(
        'bird_events_unsubscribe');

late final _UnsubscribeByOwnerDart _unsubscribeByOwner = b.nativeLib
    .lookupFunction<_UnsubscribeByOwnerNative, _UnsubscribeByOwnerDart>(
        'bird_events_unsubscribe_by_owner');

late final _DispatchQueuedDart _dispatchQueued = b.nativeLib
    .lookupFunction<_DispatchQueuedNative, _DispatchQueuedDart>(
        'bird_events_dispatch_queued');

// ---------------------------------------------------------------------------
// High-Level Dart Event API
// ---------------------------------------------------------------------------

/// Zone key holding the currently active [WorldObject] entity ID during
/// [BirdScript.init], [BirdScript.process], and [BirdScript.physicsProcess].
const Object birdActiveEntityIdZoneKey = #_birdActiveEntityId;

/// Returns the ambient owning entity ID from the current Dart [Zone], or `0`
/// if called outside a [BirdScript] lifecycle callback.
int currentZoneOwnerEntityId() {
  final Object? val = Zone.current[birdActiveEntityIdZoneKey];
  return val is int ? val : 0;
}

/// Callback signature for Dart event listeners registered via [Events.subscribe].
typedef DartEventCallback = void Function(Map<String, Object?> payload);

/// High-level Dart bridge to the engine-wide native `bird::EventBus`.
///
/// Allows Dart scripts and native C++ engine systems (physics, animation,
/// input, and gameplay code) to communicate seamlessly in both directions:
///
/// - [Events.emit] enqueues an event into the native ring buffer to be
///   dispatched at a safe point in the frame (after physics and before
///   `process(delta)`).
/// - [Events.emitImmediate] dispatches an event synchronously to all C++ and
///   Dart subscribers before returning.
/// - [Events.subscribe] registers a Dart listener and returns a subscription
///   ID. When called inside a [BirdScript] lifecycle hook (`init`, `process`,
///   `physicsProcess`), or when an explicit `owner` / `ownerEntityId` is
///   passed, the subscription is automatically cleaned up when that entity's
///   `deinit()` runs or when the entity is destroyed.
/// - [Events.unsubscribe] cancels a single subscription by ID (safe even
///   while an event is currently dispatching).
class Events {
  Events._();

  static final Map<int, DartEventCallback> _callbacks =
      <int, DartEventCallback>{};
  static ffi.NativeCallable<_NativeDispatchSig>? _dispatchCallable;
  static ffi.NativeCallable<_NativeUnsubscribeSig>? _unsubscribeCallable;

  static void _ensureRegistered() {
    if (_dispatchCallable != null) return;
    _dispatchCallable = ffi.NativeCallable<_NativeDispatchSig>.isolateLocal(
      (int subscriptionId, ffi.Pointer<ffi.Void> payloadPtr) {
        final DartEventCallback? cb = _callbacks[subscriptionId];
        if (cb == null) return;
        final Map<String, Object?> decoded = _decodeNativePayload(payloadPtr);
        cb(decoded);
      },
    );
    _unsubscribeCallable =
        ffi.NativeCallable<_NativeUnsubscribeSig>.isolateLocal(
      (int subscriptionId) {
        _callbacks.remove(subscriptionId);
      },
    );
    _registerCallbacks(
      _dispatchCallable!.nativeFunction,
      _unsubscribeCallable!.nativeFunction,
    );
  }

  /// Decodes a borrowed native `bird::EventPayload*` into a Dart map.
  static Map<String, Object?> _decodeNativePayload(
      ffi.Pointer<ffi.Void> payloadPtr) {
    if (payloadPtr == ffi.nullptr) return <String, Object?>{};
    final int count = _payloadFieldCount(payloadPtr);
    if (count == 0) return <String, Object?>{};

    return using((Arena arena) {
      final ffi.Pointer<ffi.Float> floats = arena<ffi.Float>(4);
      final Map<String, Object?> out = <String, Object?>{};
      for (int i = 0; i < count; i++) {
        final String key = _payloadFieldKey(payloadPtr, i).toDartString();
        final int type = _payloadFieldType(payloadPtr, i);
        switch (type) {
          case 0: // EventValueType::Null
            out[key] = null;
            break;
          case 1: // EventValueType::Bool
            out[key] = _payloadFieldBool(payloadPtr, i);
            break;
          case 2: // EventValueType::Int
            out[key] = _payloadFieldInt(payloadPtr, i);
            break;
          case 3: // EventValueType::Double
            out[key] = _payloadFieldDouble(payloadPtr, i);
            break;
          case 4: // EventValueType::String
            out[key] = _payloadFieldString(payloadPtr, i).toDartString();
            break;
          case 5: // EventValueType::Vector2
            _payloadFieldVec2(payloadPtr, i, floats, floats + 1);
            out[key] = Vector2(floats[0], floats[1]);
            break;
          case 6: // EventValueType::Vector3
            _payloadFieldVec3(payloadPtr, i, floats, floats + 1, floats + 2);
            out[key] = Vector3(floats[0], floats[1], floats[2]);
            break;
          case 7: // EventValueType::Vector4
            _payloadFieldVec4(
                payloadPtr, i, floats, floats + 1, floats + 2, floats + 3);
            out[key] = Vector4(floats[0], floats[1], floats[2], floats[3]);
            break;
          default:
            out[key] = null;
            break;
        }
      }
      return out;
    });
  }

  /// Populates a newly allocated native `bird::EventPayload*` from a Dart map.
  static ffi.Pointer<ffi.Void> _encodeMapPayload(
      Map<String, Object?> map, Arena arena) {
    final ffi.Pointer<ffi.Void> ptr = _payloadNew();
    for (final MapEntry<String, Object?> entry in map.entries) {
      final ffi.Pointer<Utf8> keyC =
          entry.key.toNativeUtf8(allocator: arena);
      final Object? val = entry.value;
      if (val == null) {
        _payloadSetNull(ptr, keyC);
      } else if (val is bool) {
        _payloadSetBool(ptr, keyC, val);
      } else if (val is int) {
        _payloadSetInt(ptr, keyC, val);
      } else if (val is double) {
        _payloadSetDouble(ptr, keyC, val);
      } else if (val is String) {
        final ffi.Pointer<Utf8> valC = val.toNativeUtf8(allocator: arena);
        _payloadSetString(ptr, keyC, valC);
      } else if (val is Vector2) {
        _payloadSetVec2(ptr, keyC, val.x, val.y);
      } else if (val is Vector3) {
        _payloadSetVec3(ptr, keyC, val.x, val.y, val.z);
      } else if (val is Vector4) {
        _payloadSetVec4(ptr, keyC, val.x, val.y, val.z, val.w);
      } else {
        throw ArgumentError(
          'Unsupported event payload value for key "${entry.key}": '
          '${val.runtimeType}. Supported types: null, bool, int, double, '
          'String, Vector2, Vector3, Vector4.',
        );
      }
    }
    return ptr;
  }

  /// Enqueues an event onto the native `EventBus` ring buffer for deferred
  /// dispatch at the frame's event dispatch phase (after physics, before
  /// script `process()`).
  ///
  /// [payload] may be `null`, a `Map<String, Object?>`, or a generated
  /// [EventPayload] instance.
  ///
  /// Returns `true` if the event was enqueued, or `false` if the ring buffer
  /// was full or no `EventBus` is active.
  static bool emit(String eventName, [Object? payload]) {
    return using((Arena arena) {
      final ffi.Pointer<Utf8> nameC = eventName.toNativeUtf8(allocator: arena);
      if (payload == null) {
        return _emit(nameC, ffi.nullptr);
      } else if (payload is EventPayload) {
        return _emit(nameC, payload.handle);
      } else if (payload is Map<String, Object?>) {
        final ffi.Pointer<ffi.Void> nativePayload =
            _encodeMapPayload(payload, arena);
        try {
          return _emit(nameC, nativePayload);
        } finally {
          _payloadDelete(nativePayload);
        }
      } else if (payload is Map) {
        final Map<String, Object?> typed = <String, Object?>{
          for (final MapEntry<dynamic, dynamic> e in payload.entries)
            e.key.toString(): e.value,
        };
        final ffi.Pointer<ffi.Void> nativePayload =
            _encodeMapPayload(typed, arena);
        try {
          return _emit(nameC, nativePayload);
        } finally {
          _payloadDelete(nativePayload);
        }
      } else {
        throw ArgumentError(
          'Events.emit payload must be a Map<String, Object?>, EventPayload, or null (got ${payload.runtimeType})',
        );
      }
    });
  }

  /// Synchronously dispatches an event to all C++ and Dart subscribers before
  /// returning.
  ///
  /// [payload] may be `null`, a `Map<String, Object?>`, or a generated
  /// [EventPayload] instance.
  static void emitImmediate(String eventName, [Object? payload]) {
    using((Arena arena) {
      final ffi.Pointer<Utf8> nameC = eventName.toNativeUtf8(allocator: arena);
      if (payload == null) {
        _emitImmediate(nameC, ffi.nullptr);
      } else if (payload is EventPayload) {
        _emitImmediate(nameC, payload.handle);
      } else if (payload is Map<String, Object?>) {
        final ffi.Pointer<ffi.Void> nativePayload =
            _encodeMapPayload(payload, arena);
        try {
          _emitImmediate(nameC, nativePayload);
        } finally {
          _payloadDelete(nativePayload);
        }
      } else if (payload is Map) {
        final Map<String, Object?> typed = <String, Object?>{
          for (final MapEntry<dynamic, dynamic> e in payload.entries)
            e.key.toString(): e.value,
        };
        final ffi.Pointer<ffi.Void> nativePayload =
            _encodeMapPayload(typed, arena);
        try {
          _emitImmediate(nameC, nativePayload);
        } finally {
          _payloadDelete(nativePayload);
        }
      } else {
        throw ArgumentError(
          'Events.emitImmediate payload must be a Map<String, Object?>, EventPayload, or null (got ${payload.runtimeType})',
        );
      }
    });
  }

  /// Subscribes [callback] to events matching [eventName] and returns a
  /// non-zero subscription ID.
  ///
  /// Ownership and automatic lifecycle cleanup:
  /// - If [owner] is provided, the subscription is tied to `owner.getID()`.
  /// - Otherwise, if [ownerEntityId] is provided, the subscription is tied to
  ///   that entity ID (pass `0` to force a global subscription even inside a
  ///   [BirdScript]).
  /// - Otherwise, if called inside a [BirdScript]'s `init()`, `process()`, or
  ///   `physicsProcess()`, the subscription is automatically tied to that
  ///   script's host [WorldObject] and unsubscribed when its `deinit()` runs
  ///   or when the entity is destroyed.
  static int subscribe(
    String eventName,
    DartEventCallback callback, {
    WorldObject? owner,
    int? ownerEntityId,
  }) {
    _ensureRegistered();
    final int resolvedOwner =
        owner?.getID() ?? ownerEntityId ?? currentZoneOwnerEntityId();

    return using((Arena arena) {
      final ffi.Pointer<Utf8> nameC = eventName.toNativeUtf8(allocator: arena);
      final int subId = _subscribe(nameC, resolvedOwner);
      if (subId != 0) {
        _callbacks[subId] = callback;
      }
      return subId;
    });
  }

  /// Cancels an active subscription by [subscriptionId].
  ///
  /// Safe to call at any time, including from inside an event callback while
  /// events are being dispatched. Returns `true` if the subscription was active
  /// and removed.
  static bool unsubscribe(int subscriptionId) {
    if (subscriptionId == 0) return false;
    _callbacks.remove(subscriptionId);
    return _unsubscribe(subscriptionId);
  }

  /// Cancels all subscriptions owned by [ownerEntityId] (or [owner]).
  ///
  /// Called automatically by the engine when a [WorldObject] is deinitialized
  /// or destroyed. Returns the number of subscriptions removed.
  static int unsubscribeByOwner({WorldObject? owner, int? ownerEntityId}) {
    final int id = owner?.getID() ?? ownerEntityId ?? 0;
    if (id == 0) return 0;
    return _unsubscribeByOwner(id);
  }

  /// Manually drains and dispatches all queued events currently in the native
  /// ring buffer.
  ///
  /// During normal gameplay `Application::tick()` calls this automatically once
  /// per frame (after physics and before `process(delta)`). Exposed primarily
  /// for standalone tests and custom loops.
  static int dispatchQueued() => _dispatchQueued();

  /// Number of Dart callbacks currently retained by the Dart event bridge.
  static int get activeDartCallbackCount => _callbacks.length;

  /// Closes the isolate-local native trampolines and clears all Dart event
  /// callbacks. Safe to call during `appDeinit()` or test teardown.
  static void shutdown() {
    _callbacks.clear();
    if (_dispatchCallable != null || _unsubscribeCallable != null) {
      _unregisterCallbacks();
      _dispatchCallable?.close();
      _unsubscribeCallable?.close();
      _dispatchCallable = null;
      _unsubscribeCallable = null;
    }
  }
}
