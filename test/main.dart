import 'dart:ffi' as ffi;
import 'dart:io' show Platform;
import 'dart:math' as math;
import 'package:bird_engine/bird_engine.dart';

/// Required by Dart VM loader; engine entrypoint is [appEntry].
void main() {}

/// True while the camera rides the walking character (toggle with C).
bool gWalkMode = false;

/// Keep active script instances alive for the duration of the application.
final List<BirdScript> _activeScripts = [];

/// First-person camera controller matching `POVCamera` in `test/main.cpp`.
class POVCamera extends BirdScript<Camera> {
  static const double _kSensitivity = 0.0025; // radians per pixel
  static final double _kMaxPitch = 89.0 * math.pi / 180.0;

  double _yaw = 0.0;
  double _pitch = 0.0;

  @override
  Camera wrapHost(ffi.Pointer<ffi.Void> ptr) =>
      Camera.fromHandle(ptr, owned: false);

  @override
  void init() {
    object.getWorldEnvironment().clearColor = Vector3(0.2, 0.6, 0.85);
  }

  @override
  void process(double delta) {
    final inp = inputManager;
    if (inp == null) return;

    const speed = 10.0;
    // Hold right mouse button to look around (always while walking).
    final look = gWalkMode || inp.getActionValue('look') > 0.5;
    inp.setMouseCaptured(look);
    if (look) {
      final mouse = inp.getMouseDelta();
      _yaw -= mouse.x * _kSensitivity;
      _pitch = (_pitch - mouse.y * _kSensitivity).clamp(-_kMaxPitch, _kMaxPitch);
    }

    // Yaw about world up, then pitch about camera's right axis (looks down -Z).
    object.setWorldRotation(
      fromAngleAxis(_yaw, Vector3(0.0, 1.0, 0.0)) *
          fromAngleAxis(_pitch, Vector3(1.0, 0.0, 0.0)),
    );

    if (gWalkMode) return; // character script moves the camera

    final local = Vector3(
      inp.getActionValue('move_right') - inp.getActionValue('move_left'),
      inp.getActionValue('move_up') - inp.getActionValue('move_down'),
      inp.getActionValue('move_backward') - inp.getActionValue('move_forward'),
    );

    if (local.x != 0.0 || local.y != 0.0 || local.z != 0.0) {
      final rot = object.getWorldRotation() * object.getLocalRotation();
      object.translate(rot.rotate(local) * (speed * delta));
    }
  }
}

class _BoxSpawn {
  final RigidBodyComponent body;
  final Vector3 position;
  final Quaternion rotation;

  _BoxSpawn(this.body, this.position, this.rotation);
}

/// Interactive physics playground script matching `PlaygroundScript` in `test/main.cpp`.
class PlaygroundScript extends BirdScript<WorldObject> {
  final Camera camera;
  final List<_BoxSpawn> boxes;
  final CharacterControllerComponent walker;
  final List<UILabel> hud;

  String _message = 'Physics ready';
  int _inZone = 0;
  bool _pushHeld = false;
  bool _resetHeld = false;
  bool _walkHeld = false;
  bool _debugHeld = false;

  PlaygroundScript(this.camera, this.boxes, this.walker, this.hud);

  @override
  void init() {
    hud[2].setText('F push   R reset   C walk   G colliders   RMB look');
  }

  void enterZone(ContactEvent event) {
    _inZone++;
    _message = event.otherCharacter != null
        ? 'You entered the zone'
        : 'Entered zone ($_inZone inside)';
  }

  void leaveZone(ContactEvent event) {
    if (_inZone > 0) _inZone--;
    _message = event.otherCharacter != null
        ? 'You left the zone'
        : 'Left zone ($_inZone inside)';
  }

  void walkerTouched(ContactEvent event) {
    final other = event.other;
    if (other != null && other.getMotionType() == MotionType.Dynamic) {
      _message = 'Bumped a box';
    }
  }

  @override
  void process(double delta) {
    final inp = inputManager;
    final scn = scene;
    if (inp == null || scn == null) return;

    final push = inp.getActionValue('physics_push') > 0.5;
    final reset = inp.getActionValue('physics_reset') > 0.5;
    final walk = inp.getActionValue('physics_walk') > 0.5;
    final debug = inp.getActionValue('physics_debug') > 0.5;

    if (push && !_pushHeld) _pushUnderCrosshair();
    if (reset && !_resetHeld) _resetStack();
    if (walk && !_walkHeld) _toggleWalk();
    if (debug && !_debugHeld) {
      final app = application;
      if (app != null) {
        app.setPhysicsDebugDraw(!app.isPhysicsDebugDrawEnabled());
      }
    }

    _pushHeld = push;
    _resetHeld = reset;
    _walkHeld = walk;
    _debugHeld = debug;

    if (gWalkMode) {
      camera.setWorldPosition(
        walker.getInterpolatedPosition() + Vector3(0.0, 1.6, 0.0),
      );
    }

    final world = scn.getPhysicsWorld();
    if (world != null) {
      hud[0].setText(
        'Bodies ${world.getBodyCount()}   awake ${world.getActiveBodyCount()}   in zone $_inZone',
      );
    }
    hud[1].setText(_message);
  }

  @override
  void physicsProcess(double fixedDelta) {
    final inp = inputManager;
    if (inp == null) return;

    if (!gWalkMode) {
      walker.setMoveVelocity(Vector3.zero());
      return;
    }

    // Steer in the camera's horizontal frame.
    final rotation = camera.getWorldRotation() * camera.getLocalRotation();
    var forward = rotation.rotate(Vector3(0.0, 0.0, -1.0));
    forward.y = 0.0;
    forward = forward.length > 0.0 ? forward.normalized() : Vector3(0.0, 0.0, -1.0);
    final right = Vector3(-forward.z, 0.0, forward.x);

    final ahead =
        inp.getActionValue('move_forward') - inp.getActionValue('move_backward');
    final side =
        inp.getActionValue('move_right') - inp.getActionValue('move_left');
    walker.setMoveVelocity((forward * ahead + right * side) * 4.5);

    final host = walker.world_object();
    if (host != null && host.getWorldPosition().y < -30.0) {
      walker.teleport(Vector3(0.0, -1.0, -8.0));
    }
    if (inp.getActionValue('move_up') > 0.5) {
      walker.jump(5.0);
    }
  }

  void _toggleWalk() {
    gWalkMode = !gWalkMode;
    if (gWalkMode) {
      final eye = camera.getWorldPosition();
      walker.teleport(
        Vector3(
          eye.x.clamp(-11.0, 11.0),
          -1.0,
          eye.z.clamp(-29.0, -7.0),
        ),
      );
    }
    _message = gWalkMode
        ? 'Walking: WASD, Space jumps, mouse looks'
        : 'Flying: WASD, Space/Shift up/down';
  }

  void _pushUnderCrosshair() {
    final world = scene?.getPhysicsWorld();
    if (world == null) return;

    final rotation = camera.getWorldRotation() * camera.getLocalRotation();
    final forward = rotation.rotate(Vector3(0.0, 0.0, -1.0));
    final eye = camera.getWorldPosition() + camera.getLocalPosition();

    final hit = world.raycast(eye, forward, 200.0, 0xFFFF, walker);
    if (hit == null) {
      _message = 'Push: nothing movable under the crosshair';
      return;
    }
    final body = hit.body;
    if (body == null || body.getMotionType() != MotionType.Dynamic) {
      _message = 'Push: nothing movable under the crosshair';
      hit.dispose();
      return;
    }
    body.applyImpulseAt(forward * (12.0 * body.getMass()), hit.point);
    _message = 'Push: hit at distance ${hit.distance.toInt()} m';
    hit.dispose();
  }

  void _resetStack() {
    for (final box in boxes) {
      box.body.teleport(box.position, box.rotation);
      box.body.setLinearVelocity(Vector3.zero());
      box.body.setAngularVelocity(Vector3.zero());
    }
    _message = 'Boxes dropped again';
  }
}

WorldObject _addObject(
  Scene scene,
  List<MeshRef> meshes,
  Vector3 position,
  Vector3 scale,
  RigidBodyComponent? body,
) {
  final obj = WorldObject();
  if (meshes.isNotEmpty) {
    obj.addComponent(MeshComponent.fromMeshRefList(meshes));
  }
  if (body != null) {
    obj.addComponent(body);
  }
  obj.setWorldPosition(position);
  obj.setScale(scale);
  scene.addChild(obj);
  return obj;
}

/// Builds the full physics playground matching `playground::build` in `test/main.cpp`.
void buildPlayground(Scene scene, Camera camera, UIManager ui) {
  final cube = resourceManager!.loadMesh('assets/models/cube.obj');
  final unitBox = CollisionShape.box(Vector3(1.0, 1.0, 1.0));
  final still = RigidBodySettings()..motionType = MotionType.Static;

  // Floor slab: top face at y = -2.
  _addObject(
    scene,
    cube,
    Vector3(0.0, -2.5, -18.0),
    Vector3(12.0, 0.5, 12.0),
    RigidBodyComponent(unitBox, still),
  );

  // 80 half-metre boxes dropped from a height with varied rotations.
  final boxes = <_BoxSpawn>[];
  final crateSettings = RigidBodySettings()..mass = 20.0;
  for (var y = 0; y < 5; y++) {
    for (var x = 0; x < 4; x++) {
      for (var z = 0; z < 4; z++) {
        final i = (y * 4 + x) * 4 + z;
        final pos = Vector3(
          -2.4 + x * 1.6,
          2.0 + y * 1.8,
          -21.0 + z * 1.6,
        );
        final axis = Vector3((i % 3).toDouble(), 1.0, ((i * 7) % 5).toDouble())
            .normalized();
        final rot = fromAngleAxis(0.37 * i, axis);
        final body = RigidBodyComponent(unitBox, crateSettings);
        final obj = _addObject(scene, cube, pos, Vector3(0.5, 0.5, 0.5), body);
        obj.setWorldRotation(rot);
        boxes.add(_BoxSpawn(body, pos, rot));
      }
    }
  }

  // Chain of 5 boxes hanging from a world pivot by point joints (push with F).
  final world = scene.getPhysicsWorld()!;
  final linkSettings = RigidBodySettings()..mass = 10.0;
  RigidBodyComponent? previous;
  for (var i = 0; i < 5; i++) {
    final pos = Vector3(6.0, 6.0 - i * 1.1, -14.0);
    final body = RigidBodyComponent(unitBox, linkSettings);
    _addObject(scene, cube, pos, Vector3(0.4, 0.4, 0.4), body);
    world.createPointJoint(body, previous, pos + Vector3(0.0, 0.55, 0.0));
    previous = body;
  }

  // Invisible trigger zone in front of the stack.
  final sensorSettings = RigidBodySettings()
    ..motionType = MotionType.Static
    ..isSensor = true;
  final zoneBody = RigidBodyComponent(
    CollisionShape.box(Vector3(3.0, 1.0, 2.0)),
    sensorSettings,
  );
  _addObject(
    scene,
    const [],
    Vector3(0.0, -1.0, -12.0),
    Vector3(1.0, 1.0, 1.0),
    zoneBody,
  );

  // Top-right Physics HUD.
  final hudColumn = ui.getRoot()!.add(UIStack(UIAxis.Vertical));
  hudColumn.setAnchor(UIAlignment.TopRight);
  hudColumn.setPivot(UIAlignment.TopRight);
  hudColumn.setOffset(Vector2(-16.0, 44.0));
  hudColumn.setAlign(UIAlign.End);
  hudColumn.setGap(4.0);
  final hud = <UILabel>[];
  for (var i = 0; i < 3; i++) {
    final line = hudColumn.add(UILabel(''));
    line.setColor(Vector4(1.0, 0.95, 0.6, 1.0));
    hud.add(line);
  }

  // Walking character parked on the slab until C is pressed.
  final walkerSettings = CharacterSettings()..pushStrength = 600.0;
  final walker = CharacterControllerComponent(walkerSettings);
  final walkerObj = WorldObject();
  walkerObj.addComponent(walker);
  walkerObj.setWorldPosition(Vector3(0.0, -2.0, -10.0));
  scene.addChild(walkerObj);

  final script = PlaygroundScript(camera, boxes, walker, hud);
  _activeScripts.add(script);
  zoneBody.setOnTriggerEnter((ContactEvent e) => script.enterZone(e));
  zoneBody.setOnTriggerExit((ContactEvent e) => script.leaveZone(e));
  walker.setOnContactBegin((ContactEvent e) => script.walkerTouched(e));

  final controller = WorldObject();
  controller.addComponent(script.createComponent());
  scene.addChild(controller);
}

/// Builds an interactive Dart-driven 2D UI overlay mirroring `test/main.cpp`.
void buildDartUI(UIManager ui) {
  final root = ui.getRoot()!;

  final title = root.add(UILabel('Bird Engine (Dart)'));
  title.setFontSize(42.0);
  title.setOffset(Vector2(24.0, 44.0));

  // Top menu bar
  final bar = root.add(UIMenuBar());
  bar.setAnchors(UIAlignment.TopLeft, UIAlignment.TopRight);
  bar.addMenu('File', [
    UIMenuItem.action('New scene', () => print('[Dart UI] New scene'), 'Ctrl+N'),
    UIMenuItem.separator(),
    UIMenuItem.action('Save', () => print('[Dart UI] Save'), 'Ctrl+S'),
  ]);
  bar.addMenu('Help', [
    UIMenuItem.action('About', () {
      final about = UIDialog(
        'Bird Engine (Dart)',
        'Embedded Dart 3.8.2 + Jolt Physics + 2D UI via auto-generated FFI!',
      );
      about.addButton('Close');
      ui.pushModal(about);
    }),
  ]);

  // Interactive Dart UI window
  final win = root.add(UIWindow('Dart UI Controls', Vector2(360.0, 340.0)));
  win.setOffset(Vector2(24.0, 108.0));

  final col = win.getContent()!.add(UIStack(UIAxis.Vertical));
  col.setAnchors(UIAlignment.TopLeft, UIAlignment.BottomRight);
  col.setAlign(UIAlign.Stretch);
  col.setGap(10.0);

  final status = col.add(UILabel('Interact with Dart-driven widgets below'));
  status.setColor(Vector4(0.55, 0.75, 1.0, 1.0));

  final nameBox = col.add(UITextBox());
  nameBox.setPlaceholder('Type something and press Enter');
  nameBox.setWidth(UISizeRule.fill());
  nameBox.setOnSubmit((String text) {
    status.setText('Submitted: $text');
  });

  final check = col.add(UICheckBox('Enable feature', true));
  check.setOnChanged((bool on) {
    status.setText(on ? 'Feature enabled' : 'Feature disabled');
  });

  final progress = col.add(UIProgressBar(0.35));
  final slider = col.add(UISlider(0.0, 1.0, 0.35));
  slider.setWidth(UISizeRule.fill());
  slider.setStep(0.05);
  slider.setOnChanged((double v) {
    progress.setValue(v);
    status.setText('Slider: ${(v * 100).round()}%');
  });

  final quality = col.add(UIDropdown(['Low', 'Medium', 'High', 'Ultra']));
  quality.setWidth(UISizeRule.fill());
  quality.setSelectedIndex(2);
  quality.setOnChanged((int idx) {
    status.setText('Quality: ${quality.getSelectedText()}');
  });

  final btnRow = col.add(UIStack(UIAxis.Horizontal));
  btnRow.setGap(8.0);
  final saveBtn = btnRow.add(UIButton('Save'));
  saveBtn.setOnClick(() => status.setText('Saved from Dart!'));

  final dlgBtn = btnRow.add(UIButton('Open Dialog'));
  dlgBtn.setOnClick(() {
    final dlg = UIDialog('Confirm', 'Triggered from a Dart closure!');
    dlg.addButton('Cancel', () => status.setText('Cancelled'));
    dlg.addButton('OK', () => status.setText('Confirmed!'));
    ui.pushModal(dlg);
  });
}

/// Called by `Bird` when launching `test/main.dart`.
@pragma('vm:entry-point')
void appEntry(int sceneAddr) {
  final inp = input!;
  inp.addAction(Action('move_forward')..addKey(KEY_W));
  inp.addAction(Action('move_backward')..addKey(KEY_S));
  inp.addAction(Action('move_left')..addKey(KEY_A));
  inp.addAction(Action('move_right')..addKey(KEY_D));
  inp.addAction(Action('move_up')..addKey(KEY_SPACE));
  inp.addAction(Action('move_down')..addKey(KEY_LEFT_SHIFT));
  inp.addAction(Action('look')..addMouseButton(MOUSE_BUTTON_2));
  inp.addAction(Action('physics_debug')..addKey(KEY_G));
  inp.addAction(Action('physics_push')..addKey(KEY_F));
  inp.addAction(Action('physics_reset')..addKey(KEY_R));
  inp.addAction(Action('physics_walk')..addKey(KEY_C));

  final rootScene = sceneFromAddress(sceneAddr);
  final rm = resourceManager!;

  // Village map: visual + 3x3 static triangle-mesh collision chunks.
  final mapMeshes = rm.loadMesh('assets/models/maps/Map1.glb');
  final mapPosition = Vector3(0.0, -5.0, 30.0);
  final mapEntity = WorldObject();
  mapEntity.addComponent(MeshComponent.fromMeshRefList(mapMeshes));
  mapEntity.translate(mapPosition);
  rootScene.addChild(mapEntity);

  final levelSettings = RigidBodySettings()..motionType = MotionType.Static;
  for (final chunk in CollisionShape.triangleMeshChunks(mapMeshes, 3, 3)) {
    final collider = WorldObject();
    collider.addComponent(RigidBodyComponent(chunk, levelSettings));
    collider.setWorldPosition(mapPosition);
    rootScene.addChild(collider);
  }

  final cam = Camera(1280, 720);
  final pov = POVCamera();
  _activeScripts.add(pov);
  cam.addComponent(pov.createComponent());
  cam.enable();
  rootScene.addChild(cam);

  final ui = application?.getUIManager();
  if (ui != null) {
    buildDartUI(ui);
    buildPlayground(rootScene, cam, ui);
  }

  if (Platform.environment.containsKey('BIRD_PHYSICS_DEBUG')) {
    application?.setPhysicsDebugDraw(true);
  }
}

@pragma('vm:entry-point')
void appDeinit() {
  _activeScripts.clear();
  clearNativeCallbacks();
}
