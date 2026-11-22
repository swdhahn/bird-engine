import 'package:bird_engine/bird_engine.dart';

void main() {}

class _TestScript extends BirdScript<WorldObject> {
  int initCount = 0;
  int processCount = 0;
  double totalDelta = 0.0;
  int deinitCount = 0;

  @override
  void init() {
    initCount++;
    object.translate(Vector3(10.0, 0.0, 0.0));
  }

  @override
  void process(double delta) {
    processCount++;
    totalDelta += delta;
    object.translate(Vector3(0.0, delta, 0.0));
  }

  @override
  void deinit() {
    deinitCount++;
  }
}

/// Invoked from DartIntegrationTest.cpp inside the embedded Dart VM.
/// Runs end-to-end FFI round-trips against live C++ Bird Engine objects.
@pragma('vm:entry-point')
void runIntegrationChecks(int externalSceneAddr) {
  // 1. Create Scene and WorldObject from Dart (owned by Dart)
  final scene = Scene();
  if (!scene.isOwned) throw StateError('Scene should start owned by Dart');
  if (scene.getType() != OBJ_TYPES.SCENE) {
    throw StateError('Expected OBJ_TYPES.SCENE');
  }

  // Inherited WorldObject method on Scene
  scene.translate(Vector3(1.0, 2.0, 3.0));
  final scenePos = scene.getWorldPosition();
  if ((scenePos.x - 1.0).abs() > 1e-4 ||
      (scenePos.y - 2.0).abs() > 1e-4 ||
      (scenePos.z - 3.0).abs() > 1e-4) {
    throw StateError('Unexpected scene position: $scenePos');
  }

  // 2. Ownership transfer via Scene.addChild(_own<WorldObject>)
  final child = WorldObject();
  final childId = child.getID();
  child.translate(Vector3(4.0, 5.0, 6.0));
  if (!child.isOwned) throw StateError('Child should start owned');

  scene.addChild(child);
  if (child.isOwned) {
    throw StateError('Child ownership should have transferred to C++ Scene');
  }

  final children = scene.getChildren();
  if (children.length != 1) {
    throw StateError('Expected 1 child, got ${children.length}');
  }
  if (children[0].isOwned) {
    throw StateError('Borrowed child from getChildren() must not be owned');
  }
  if (children[0].getID() != childId) {
    throw StateError('Child ID mismatch');
  }

  // 3. Camera + WorldEnvironment public field getter/setter
  final cam = Camera(1280, 720);
  if (cam.getType() != OBJ_TYPES.CAMERA) {
    throw StateError('Expected OBJ_TYPES.CAMERA');
  }
  final env = cam.getWorldEnvironment();
  if (env.isOwned) {
    throw StateError('Borrowed WorldEnvironment must not be owned');
  }
  env.clearColor = Vector3(0.2, 0.6, 0.85);
  final readBack = cam.getWorldEnvironment().clearColor;
  if ((readBack.x - 0.2).abs() > 1e-4 ||
      (readBack.y - 0.6).abs() > 1e-4 ||
      (readBack.z - 0.85).abs() > 1e-4) {
    throw StateError('WorldEnvironment.clearColor mismatch: $readBack');
  }
  scene.addChild(cam);

  // 4. std::string + Action + input constants + vector<int> round-trip
  final action = Action('move_forward');
  action.addKey(KEY_W).addKey(KEY_UP);
  if (action.getName() != 'move_forward') {
    throw StateError('Action name mismatch: ${action.getName()}');
  }
  final keys = action.getKeys();
  if (keys.length != 2 || keys[0] != KEY_W || keys[1] != KEY_UP) {
    throw StateError('Action keys mismatch: $keys');
  }
  action.dispose();

  // 5. Free math functions
  final inside = contains(Vector2(5.0, 5.0), Vector4(0.0, 0.0, 10.0, 10.0));
  final outside = contains(Vector2(15.0, 5.0), Vector4(0.0, 0.0, 10.0, 10.0));
  if (!inside || outside) {
    throw StateError('contains() returned unexpected results');
  }

  // 6. Borrowed C++ Scene passed in from C++ test + BirdScript NativeCallable
  final extScene = sceneFromAddress(externalSceneAddr);
  if (extScene.isOwned) {
    throw StateError('External C++ scene must be borrowed');
  }
  final scriptHost = WorldObject();
  final script = _TestScript();
  scriptHost.addComponent(script.createComponent());
  extScene.addChild(scriptHost);
  // Adding to initialized scene invokes init(), which translates X by +10.
  // Verify via C++ side after returning as well.
  if (script.initCount != 1) {
    throw StateError('Expected BirdScript.init() to be called once, got ${script.initCount}');
  }
  final hostPos = scriptHost.getWorldPosition();
  if ((hostPos.x - 10.0).abs() > 1e-4) {
    throw StateError('Expected scriptHost.x == 10.0, got ${hostPos.x}');
  }

  // Cleanly dispose Dart-owned scene (destroys child + cam too)
  scene.dispose();
}

/// Tests Pass 2 2D UI widgets, layout enums, UIAlignment static constexprs,
/// std::vector<std::string>, and std::function Dart closure callbacks.
@pragma('vm:entry-point')
void runUiIntegrationChecks() {
  final root = UIObject();
  final stack = root.add(UIStack(UIAxis.Vertical));
  stack.setAnchors(UIAlignment.TopLeft, UIAlignment.BottomRight);
  stack.setAlign(UIAlign.Stretch);
  stack.setGap(8.0);

  final status = stack.add(UILabel('Initial'));

  // 1. UIButton + std::function<void()>
  var clickCount = 0;
  final btn = stack.add(UIButton('Save'));
  btn.setOnClick(() {
    clickCount++;
    status.setText('Clicked $clickCount');
  });
  btn.click();
  if (clickCount != 1 || status.getText() != 'Clicked 1') {
    throw StateError('UIButton onClick failed: count=$clickCount text=${status.getText()}');
  }

  // 2. UISlider + std::function<void(float)>
  var sliderVal = 0.0;
  final slider = stack.add(UISlider(0.0, 1.0, 0.25));
  slider.setOnChanged((double v) {
    sliderVal = v;
  });
  slider.setValue(0.75);
  if ((sliderVal - 0.75).abs() > 1e-4) {
    throw StateError('UISlider onChanged failed: $sliderVal');
  }

  // 3. UICheckBox + std::function<void(bool)>
  bool? checkedState;
  final check = stack.add(UICheckBox('Shadows', true));
  check.setOnChanged((bool on) {
    checkedState = on;
  });
  check.setChecked(false);
  if (checkedState != false) {
    throw StateError('UICheckBox onChanged failed: $checkedState');
  }

  // 4. UIDropdown + std::vector<std::string> + std::function<void(size_t)>
  var selectedIdx = -1;
  final dropdown = stack.add(UIDropdown(['Low', 'Medium', 'High']));
  dropdown.setOnChanged((int idx) {
    selectedIdx = idx;
  });
  dropdown.setSelectedIndex(2);
  if (selectedIdx != 2 || dropdown.getSelectedText() != 'High') {
    throw StateError(
      'UIDropdown failed: idx=$selectedIdx text=${dropdown.getSelectedText()}',
    );
  }
  final items = dropdown.getItems();
  if (items.length != 3 || items[1] != 'Medium') {
    throw StateError('UIDropdown.getItems() mismatch: $items');
  }

  // 5. UITextBox + std::function<void(const std::string&)>
  var textChanged = '';
  final box = stack.add(UITextBox('hello'));
  box.setOnChanged((String s) {
    textChanged = s;
  });
  box.setText('bird');
  if (box.getText() != 'bird') {
    throw StateError('UITextBox text mismatch: ${box.getText()} (cb=$textChanged)');
  }

  // 6. UIMenuItem + std::vector<UIMenuItem>
  final menuItem = UIMenuItem.action('Reset', () {}, 'Ctrl+R');
  root.setContextMenu([menuItem, UIMenuItem.separator()]);

  root.dispose();
}

/// Tests Jolt Physics from Dart: PhysicsServer, PhysicsWorld, CollisionShape,
/// RigidBodyComponent, gravity simulation, raycast(), and onContactBegin callbacks.
@pragma('vm:entry-point')
void runPhysicsIntegrationChecks() {
  final server = PhysicsServer(PhysicsServerSettings());
  final scene = Scene();
  final world = scene.createPhysicsWorld(PhysicsWorldSettings())!;
  scene.init(null);

  // Static floor at y = -0.5 (top surface at y = 0.0)
  final floorObj = WorldObject();
  floorObj.setWorldPosition(Vector3(0.0, -0.5, 0.0));
  final floorSettings = RigidBodySettings()..motionType = MotionType.Static;
  final floorBody = RigidBodyComponent(
    CollisionShape.box(Vector3(10.0, 0.5, 10.0)),
    floorSettings,
  );
  floorObj.addComponent(floorBody);
  scene.addChild(floorObj);

  // Dynamic box dropped from y = 2.0
  var contactCount = 0;
  final boxObj = WorldObject();
  boxObj.setWorldPosition(Vector3(0.0, 2.0, 0.0));
  final boxSettings = RigidBodySettings()..motionType = MotionType.Dynamic;
  final boxBody = RigidBodyComponent(
    CollisionShape.box(Vector3(0.5, 0.5, 0.5)),
    boxSettings,
  );
  boxBody.setOnContactBegin((ContactEvent ev) {
    contactCount++;
  });
  boxObj.addComponent(boxBody);
  scene.addChild(boxObj);

  if (world.getBodyCount() != 2) {
    throw StateError('Expected 2 rigid bodies, got ${world.getBodyCount()}');
  }

  // Simulate 1 second at 60 Hz
  for (var i = 0; i < 60; i++) {
    world.step(1.0 / 60.0);
  }

  final finalPos = boxObj.getWorldPosition();
  if (finalPos.y >= 1.5 || finalPos.y < 0.0) {
    throw StateError('Box should have landed near y=0.5, got y=${finalPos.y}');
  }
  if (contactCount < 1) {
    throw StateError('Expected onContactBegin callback to fire at least once');
  }

  // Raycast straight down from y = 5.0 (omitting optional ignore parameter)
  final hit = world.raycast(
    Vector3(0.0, 5.0, 0.0),
    Vector3(0.0, -1.0, 0.0),
    20.0,
    0xFFFF,
  );
  if (hit == null) {
    throw StateError('Expected raycast to hit the fallen box');
  }
  hit.dispose();

  scene.deinit();
  scene.dispose();
  floorSettings.dispose();
  boxSettings.dispose();
  server.dispose();
}

int _cppToDartImmediateScore = 0;
int _cppToDartQueuedScore = 0;
int _cppToDartSubId = 0;

class _EventListenerScript extends BirdScript<WorldObject> {
  int hits = 0;
  int lastAmount = 0;

  @override
  void init() {
    // Subscribed inside BirdScript.init() without explicit owner:
    // automatically bound to object.getID() via Zone and cleaned up on deinit!
    Events.subscribe('entity.ping', (payload) {
      hits++;
      lastAmount = (payload['amount'] as int?) ?? 0;
    });
  }
}

/// Tests Dart Events API: queued vs immediate dispatch, self-unsubscribe during
/// dispatch, cross-boundary Dart -> C++ emission, C++ -> Dart reception,
/// native physics -> Dart event bus delivery, and automatic entity cleanup.
@pragma('vm:entry-point')
void runEventBusIntegrationChecks() {
  // 1. Queued vs Immediate order in Dart
  final order = <String>[];
  final subOrder = Events.subscribe('dart.order', (payload) {
    order.add(payload['step'] as String);
  });

  Events.emit('dart.order', {'step': 'queued-1'});
  Events.emit('dart.order', {'step': 'queued-2'});
  if (order.isNotEmpty) {
    throw StateError('Queued events must not fire before dispatchQueued()');
  }

  Events.emitImmediate('dart.order', {'step': 'immediate-1'});
  if (order.length != 1 || order[0] != 'immediate-1') {
    throw StateError('Expected immediate-1 first, got $order');
  }

  final dispatched = Events.dispatchQueued();
  if (dispatched != 2 ||
      order.length != 3 ||
      order[1] != 'queued-1' ||
      order[2] != 'queued-2') {
    throw StateError('Unexpected queued dispatch order: $order');
  }
  Events.unsubscribe(subOrder);

  // 2. Full payload types round-trip (null, bool, int, double, String, Vector2/3/4)
  Map<String, Object?>? receivedPayload;
  final subTypes = Events.subscribe('dart.types', (payload) {
    receivedPayload = payload;
  });
  Events.emitImmediate('dart.types', {
    'nil': null,
    'b': true,
    'i': -123456789,
    'd': 3.14159,
    's': 'feather',
    'v2': Vector2(1.0, 2.0),
    'v3': Vector3(3.0, 4.0, 5.0),
    'v4': Vector4(6.0, 7.0, 8.0, 9.0),
  });
  Events.unsubscribe(subTypes);

  final p = receivedPayload;
  if (p == null ||
      !p.containsKey('nil') ||
      p['nil'] != null ||
      p['b'] != true ||
      p['i'] != -123456789 ||
      ((p['d'] as double) - 3.14159).abs() > 1e-6 ||
      p['s'] != 'feather' ||
      (p['v2'] as Vector2).y != 2.0 ||
      (p['v3'] as Vector3).z != 5.0 ||
      (p['v4'] as Vector4).w != 9.0) {
    throw StateError('Payload round-trip mismatch: $p');
  }

  // 3. Safe unsubscribe during dispatch
  var selfUnsubCalls = 0;
  var siblingCalls = 0;
  late int selfId;
  late int siblingId;
  selfId = Events.subscribe('dart.unsub', (_) {
    selfUnsubCalls++;
    Events.unsubscribe(selfId);
    Events.unsubscribe(siblingId);
  });
  siblingId = Events.subscribe('dart.unsub', (_) {
    siblingCalls++;
  });
  Events.emitImmediate('dart.unsub');
  Events.emitImmediate('dart.unsub');
  if (selfUnsubCalls != 1 || siblingCalls != 0) {
    throw StateError(
      'Safe mid-dispatch unsubscribe failed: self=$selfUnsubCalls sibling=$siblingCalls',
    );
  }

  // 4. Automatic lifecycle cleanup when BirdScript entity is removed / deinitialized
  final scene = Scene();
  scene.init(null);

  final entity = WorldObject();
  final listenerScript = _EventListenerScript();
  entity.addComponent(listenerScript.createComponent());
  scene.addChild(entity);

  final callbacksBefore = Events.activeDartCallbackCount;
  Events.emitImmediate('entity.ping', {'amount': 42});
  if (listenerScript.hits != 1 || listenerScript.lastAmount != 42) {
    throw StateError(
      'Expected _EventListenerScript to receive entity.ping once, got ${listenerScript.hits}',
    );
  }

  // Removing the child runs WorldObject::deinit(), which unsubscribes all
  // listeners owned by entity.getID() and invokes the Dart unsubscribe hook.
  scene.removeChild(entity);
  if (Events.activeDartCallbackCount >= callbacksBefore) {
    throw StateError(
      'Expected Dart callback map to shrink on entity deinit '
      '(before=$callbacksBefore, after=${Events.activeDartCallbackCount})',
    );
  }

  Events.emitImmediate('entity.ping', {'amount': 99});
  if (listenerScript.hits != 1 || listenerScript.lastAmount != 42) {
    throw StateError(
      'Destroyed entity must NOT receive events after removeChild/deinit!',
    );
  }

  scene.deinit();
  scene.dispose();

  // 5. Native Physics -> Dart EventBus subscription ('physics.contact_begin')
  final physServer = PhysicsServer(PhysicsServerSettings());
  final physScene = Scene();
  final physWorld = physScene.createPhysicsWorld(PhysicsWorldSettings())!;
  physScene.init(null);

  final floorObj = WorldObject()..setWorldPosition(Vector3(0.0, -0.5, 0.0));
  final floorSettings = RigidBodySettings()..motionType = MotionType.Static;
  floorObj.addComponent(
    RigidBodyComponent(
      CollisionShape.box(Vector3(10.0, 0.5, 10.0)),
      floorSettings,
    ),
  );
  physScene.addChild(floorObj);

  final boxObj = WorldObject()..setWorldPosition(Vector3(0.0, 1.5, 0.0));
  final boxSettings = RigidBodySettings()..motionType = MotionType.Dynamic;
  boxObj.addComponent(
    RigidBodyComponent(
      CollisionShape.box(Vector3(0.5, 0.5, 0.5)),
      boxSettings,
    ),
  );
  physScene.addChild(boxObj);

  var busContactCount = 0;
  final physSub = Events.subscribe('physics.contact_begin', (payload) {
    if (payload.containsKey('selfEntityId') &&
        payload.containsKey('otherEntityId') &&
        payload['point'] is Vector3) {
      busContactCount++;
    }
  });

  for (var i = 0; i < 60; i++) {
    physWorld.step(1.0 / 60.0);
    Events.dispatchQueued();
  }
  Events.unsubscribe(physSub);
  if (busContactCount < 1) {
    throw StateError(
      'Expected Dart Events subscriber to receive physics.contact_begin from native Jolt step',
    );
  }

  physScene.deinit();
  physScene.dispose();
  floorSettings.dispose();
  boxSettings.dispose();
  physServer.dispose();

  // 6. Subscribe to 'cpp.to_dart' and emit 'dart.to_cpp.*' back to C++
  _cppToDartSubId = Events.subscribe('cpp.to_dart', (payload) {
    final origin = payload['origin'] as String?;
    final score = (payload['score'] as int?) ?? 0;
    if (origin == 'cpp-immediate') {
      _cppToDartImmediateScore = score;
    } else if (origin == 'cpp-queued') {
      _cppToDartQueuedScore = score;
    }
  });

  Events.emitImmediate('dart.to_cpp.immediate', {
    'tag': 'from-dart',
    'damage': 77,
    'pos': Vector3(1.5, -2.5, 3.5),
  });
  Events.emit('dart.to_cpp.queued', {'queued': true});
}

@pragma('vm:entry-point')
void verifyCppToDartEventsAndCleanup() {
  if (_cppToDartImmediateScore != 500) {
    throw StateError(
      'Expected cpp-immediate score 500, got $_cppToDartImmediateScore',
    );
  }
  if (_cppToDartQueuedScore != 1000) {
    throw StateError(
      'Expected cpp-queued score 1000, got $_cppToDartQueuedScore',
    );
  }
  Events.unsubscribe(_cppToDartSubId);
  Events.shutdown();
}
