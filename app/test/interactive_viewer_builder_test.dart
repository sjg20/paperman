import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';

void main() {
  testWidgets('builder viewport Quad matches transform', (tester) async {
    final controller = TransformationController();
    double? lastQuadTop;
    double? lastQuadBottom;
    double? lastTy;
    double? lastScale;

    await tester.pumpWidget(
      MaterialApp(
        home: Scaffold(
          body: LayoutBuilder(
            builder: (context, constraints) {
              return InteractiveViewer.builder(
                transformationController: controller,
                minScale: 1.0,
                maxScale: 5.0,
                builder: (context, viewport) {
                  lastQuadTop = viewport.point0.y;
                  lastQuadBottom = viewport.point2.y;
                  lastTy = controller.value.getTranslation().y;
                  lastScale = controller.value.getMaxScaleOnAxis();
                  return SizedBox(
                    width: constraints.maxWidth,
                    height: 6000,
                    child: const Placeholder(),
                  );
                },
              );
            },
          ),
        ),
      ),
    );
    await tester.pump();

    final viewHeight = tester.getSize(find.byType(Scaffold)).height -
        kToolbarHeight -
        MediaQuery.of(tester.element(find.byType(Scaffold))).padding.top;

    // --- Initial state ---
    debugPrint('Initial: quadTop=$lastQuadTop quadBottom=$lastQuadBottom '
        'ty=$lastTy scale=$lastScale viewHeight=$viewHeight');
    expect(lastQuadTop, closeTo(0, 1));

    // --- Pan down: set transform to translate(0, -600) ---
    controller.value = Matrix4.identity()..translate(0.0, -600.0);
    await tester.pump();

    debugPrint('After translate(0,-600): quadTop=$lastQuadTop '
        'quadBottom=$lastQuadBottom ty=$lastTy scale=$lastScale');

    // childTop from my formula: -(-600)/1 = 600
    final myChildTop = -lastTy! / lastScale!;
    debugPrint('My childTop: $myChildTop, Quad top: $lastQuadTop');
    expect(myChildTop, closeTo(lastQuadTop!, 1),
        reason: 'transform-derived childTop should match Quad top');

    // --- Pan further: translate(0, -1800) ---
    controller.value = Matrix4.identity()..translate(0.0, -1800.0);
    await tester.pump();

    debugPrint('After translate(0,-1800): quadTop=$lastQuadTop '
        'quadBottom=$lastQuadBottom ty=$lastTy scale=$lastScale');

    final myChildTop2 = -lastTy! / lastScale!;
    debugPrint('My childTop: $myChildTop2, Quad top: $lastQuadTop');
    expect(myChildTop2, closeTo(lastQuadTop!, 1),
        reason: 'transform-derived childTop should match Quad top');

    // --- Zoom to 2x then pan ---
    controller.value = Matrix4.identity()
      ..translate(0.0, -1200.0)
      ..scale(2.0);
    await tester.pump();

    debugPrint('After translate(0,-1200)*scale(2): quadTop=$lastQuadTop '
        'quadBottom=$lastQuadBottom ty=$lastTy scale=$lastScale');

    final myChildTop3 = -lastTy! / lastScale!;
    debugPrint('My childTop: $myChildTop3, Quad top: $lastQuadTop');
    expect(myChildTop3, closeTo(lastQuadTop!, 1),
        reason: 'transform-derived childTop should match Quad top at 2x');

    controller.dispose();
  });
}
