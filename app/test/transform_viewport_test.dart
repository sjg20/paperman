import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';

void main() {
  // Reproduce the child-top calculation used in the builder.
  double childTopFromMatrix(Matrix4 matrix) {
    final scale = matrix.getMaxScaleOnAxis();
    final ty = matrix.getTranslation().y;
    return -ty / scale;
  }

  test('identity transform → childTop 0', () {
    final m = Matrix4.identity();
    expect(childTopFromMatrix(m), 0.0);
  });

  test('translate(0, -600) → childTop 600', () {
    final m = Matrix4.identity()..translateByDouble(0.0, -600.0, 0.0, 1.0);
    expect(childTopFromMatrix(m), 600.0);
  });

  test('translate(0, -1200) → childTop 1200', () {
    final m = Matrix4.identity()..translateByDouble(0.0, -1200.0, 0.0, 1.0);
    expect(childTopFromMatrix(m), 1200.0);
  });

  test('scale 2x at origin then translate', () {
    // translate(0, -1200) * scale(2)
    // Matrix is: first scale by 2, then translate by (0, -1200)
    // A child point (0, y) → (0, 2*y - 1200)
    // For viewport top (y_viewport=0): 2*y - 1200 = 0 → y = 600
    // So childTop should be 600.
    final m = Matrix4.identity()
      ..translateByDouble(0.0, -1200.0, 0.0, 1.0)
      ..scaleByDouble(2.0, 2.0, 2.0, 1.0);
    expect(childTopFromMatrix(m), closeTo(600.0, 0.01));
  });

  test('_jumpToPage matrix construction', () {
    // _jumpToPage(3, 600) at scale 1:
    // Matrix4.identity()..translate(0, -1*1200)..scale(1)
    // translate(0, -1200) * scale(1) = translate(0, -1200)
    const page = 3;
    const extent = 600.0;
    const scale = 1.0;
    final m = Matrix4.identity()
      ..translateByDouble(0.0, -scale * (page - 1) * extent, 0.0, 1.0)
      ..scaleByDouble(scale, scale, scale, 1.0);

    expect(childTopFromMatrix(m), closeTo(1200.0, 0.01));
  });

  test('InteractiveViewer-style incremental panning', () {
    // Simulate what InteractiveViewer does: left-multiply translation deltas
    // Pan down 300px, then another 300px.
    var m = Matrix4.identity();

    // First pan: child should move up by 300
    final delta1 = Matrix4.identity()..translateByDouble(0.0, -300.0, 0.0, 1.0);
    m = delta1 * m;
    expect(childTopFromMatrix(m), closeTo(300.0, 0.01));

    // Second pan: child should move up by another 300
    final delta2 = Matrix4.identity()..translateByDouble(0.0, -300.0, 0.0, 1.0);
    m = delta2 * m;
    expect(childTopFromMatrix(m), closeTo(600.0, 0.01));
  });

  test('InteractiveViewer-style pan at 2x zoom', () {
    // Start with scale 2x at origin
    var m = Matrix4.identity()..scaleByDouble(2.0, 2.0, 2.0, 1.0);

    // At 2x zoom, panning by 300 viewport pixels moves 150 child pixels.
    // But InteractiveViewer translates in viewport space, so:
    // m = translate(0, -300) * scale(2)
    // getTranslation().y = -300, scale = 2
    // childTop = 300/2 = 150
    final delta = Matrix4.identity()..translateByDouble(0.0, -300.0, 0.0, 1.0);
    m = delta * m;

    expect(childTopFromMatrix(m), closeTo(150.0, 0.01));
  });
}
