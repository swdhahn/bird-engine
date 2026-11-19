import 'dart:ffi' as ffi;
import 'package:bird_engine/bird_engine.dart';

/// Required by Dart VM loader; engine entrypoint is [appEntry].
void main() {}

/// First-person camera controller mirroring `POVCamera` in `test/main.cpp`.
class POVCamera extends BirdScript<Camera> {
  @override
  Camera wrapHost(ffi.Pointer<ffi.Void> ptr) =>
      Camera.fromHandle(ptr, owned: false);

  @override
  void init() {
    object.getWorldEnvironment().clearColor = Vector3(0.2, 0.6, 0.85);
    print('[Dart] POVCamera initialized (clearColor set to sky blue)');
  }

  @override
  void process(double delta) {
    final inp = inputManager;
    if (inp == null) return;

    const speed = 10.0;
    const rotSpeed = 5.0;

    final rotVal =
        inp.getActionValue('look_right') - inp.getActionValue('look_left');
    if (rotVal != 0.0) {
      object.rotateLocal(Vector3(0.0, 1.0, 0.0), rotVal * rotSpeed * delta);
    }

    final move = Vector3(
      (-inp.getActionValue('move_right') + inp.getActionValue('move_left')) *
          speed *
          delta,
      (-inp.getActionValue('move_up') + inp.getActionValue('move_down')) *
          speed *
          delta,
      (-inp.getActionValue('move_backward') +
              inp.getActionValue('move_forward')) *
          speed *
          delta,
    );

    if (move.x != 0.0 || move.y != 0.0 || move.z != 0.0) {
      // Matches GLM `move * localRot * worldRot` == `inverse(localRot * worldRot) * move`.
      final rot =
          (object.getLocalRotation() * object.getWorldRotation()).conjugate();
      object.translate(rot.rotate(move));
    }
  }
}

/// Continuously rotates its host entity to demonstrate per-frame Dart ticking.
class SpinningCube extends BirdScript<WorldObject> {
  @override
  void init() {
    print('[Dart] SpinningCube initialized on entity #${object.getID()}');
  }

  @override
  void process(double delta) {
    object.rotateLocal(Vector3(0.0, 1.0, 0.0), 1.2 * delta);
    object.rotateLocal(Vector3(1.0, 0.0, 0.0), 0.6 * delta);
  }
}

// Keep active script instances alive for the duration of the application.
final List<BirdScript> _activeScripts = [];

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
        'Running embedded Dart 3.8.2 with auto-generated FFI bindings!',
      );
      about.addButton('Close');
      ui.pushModal(about);
    }),
  ]);

  // Interactive Dart UI Gallery window
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
  print('[Dart] appEntry() started!');

  final inp = input!;
  inp.addAction(Action('move_forward')..addKey(KEY_W));
  inp.addAction(Action('move_backward')..addKey(KEY_S));
  inp.addAction(Action('move_left')..addKey(KEY_A));
  inp.addAction(Action('move_right')..addKey(KEY_D));
  inp.addAction(Action('move_up')..addKey(KEY_SPACE));
  inp.addAction(Action('move_down')..addKey(KEY_LEFT_SHIFT));
  inp.addAction(Action('look_up')..addKey(KEY_UP));
  inp.addAction(Action('look_down')..addKey(KEY_DOWN));
  inp.addAction(Action('look_left')..addKey(KEY_LEFT));
  inp.addAction(Action('look_right')..addKey(KEY_RIGHT));

  final rootScene = sceneFromAddress(sceneAddr);
  final rm = resourceManager!;

  // 1. Load Map1.glb via ResourceManager::loadMesh -> MeshComponent
  final mapEntity = WorldObject();
  mapEntity.addComponent(
    MeshComponent.fromMeshRefList(rm.loadMesh('assets/models/maps/Map1.glb')),
  );
  mapEntity.translate(Vector3(0.0, -5.0, 30.0));
  rootScene.addChild(mapEntity);

  // 2. Load spinning cube.obj
  final cubeEntity = WorldObject();
  cubeEntity.addComponent(
    MeshComponent.fromMeshRefList(rm.loadMesh('assets/models/cube.obj')),
  );
  final spinner = SpinningCube();
  _activeScripts.add(spinner);
  cubeEntity.addComponent(spinner.createComponent());
  cubeEntity.translate(Vector3(0.0, 5.0, 0.0));
  rootScene.addChild(cubeEntity);

  // 3. Create POV camera
  final cam = Camera(1280, 720);
  final pov = POVCamera();
  _activeScripts.add(pov);
  cam.addComponent(pov.createComponent());
  cam.enable();
  rootScene.addChild(cam);

  // 4. Build interactive 2D UI overlay
  final ui = application?.getUIManager();
  if (ui != null) {
    buildDartUI(ui);
  }

  print(
    '[Dart] Scene setup complete (${rootScene.getChildren().length} root children).',
  );
}

@pragma('vm:entry-point')
void appDeinit() {
  print('[Dart] appDeinit() called.');
  _activeScripts.clear();
  clearNativeCallbacks();
}
