import 'package:flutter_test/flutter_test.dart';
import 'package:paperman/screens/viewer_screen.dart';

(int, int) range({
  required double top,
  required double bottom,
  required double extent,
  int pageCount = 10,
}) =>
    ViewerScreen.visibleRange(
      viewportTop: top,
      viewportBottom: bottom,
      extent: extent,
      pageCount: pageCount,
    );

void main() {
  // Use a round extent for easy reasoning.
  const extent = 600.0;
  const viewHeight = 800.0;

  test('initial position shows first pages', () {
    final (first, last) = range(top: 0, bottom: viewHeight, extent: extent);
    expect(first, 0); // page 1
    expect(last, 2); // ceil(800/600) = 2 → page 3
  });

  test('scrolled exactly one page down', () {
    final (first, last) = range(
      top: extent,
      bottom: extent + viewHeight,
      extent: extent,
    );
    expect(first, 1); // page 2
    expect(last, 3); // ceil(1400/600) = 3 → page 4
  });

  test('scrolled half a page down', () {
    final (first, last) = range(
      top: extent / 2,
      bottom: extent / 2 + viewHeight,
      extent: extent,
    );
    expect(first, 0); // floor(300/600) = 0 → page 1
    expect(last, 2); // ceil(1100/600) = 2 → page 3
  });

  test('at the very bottom of a 10-page document', () {
    const totalHeight = extent * 10;
    final top = totalHeight - viewHeight;
    final (first, last) = range(
      top: top,
      bottom: totalHeight,
      extent: extent,
    );
    expect(last, 9); // page 10 (last 0-based index)
    expect(first, lessThanOrEqualTo(8));
  });

  test('clamped to valid range', () {
    final (first, last) = range(
      top: -100,
      bottom: 200,
      extent: extent,
    );
    expect(first, 0);
    expect(last, greaterThanOrEqualTo(0));
  });

  test('zoomed in 2x sees fewer pages', () {
    // At 2x zoom, viewport covers half the child-space height.
    final (first, last) = range(
      top: 0,
      bottom: viewHeight / 2,
      extent: extent,
    );
    expect(first, 0);
    expect(last, 1); // ceil(400/600) = 1
  });

  test('transform-derived coordinates two pages down', () {
    // Simulate: transform translate(0, -1200) at scale 1
    // childTop = -(-1200)/1 = 1200
    // childBottom = 1200 + 800 = 2000
    final (first, last) = range(
      top: 1200,
      bottom: 2000,
      extent: extent,
    );
    expect(first, 2); // floor(1200/600) = 2 → page 3
    expect(last, 4); // ceil(2000/600) = 4 → page 5
  });

  test('pages at exact extent boundaries', () {
    // Top is exactly at page 3 boundary (index 2)
    final (first, last) = range(
      top: 2 * extent,
      bottom: 2 * extent + viewHeight,
      extent: extent,
    );
    expect(first, 2); // page 3
    expect(last, 4); // ceil((1200+800)/600) = ceil(3.33) = 4
  });

  test('single page document', () {
    final (first, last) = range(
      top: 0,
      bottom: viewHeight,
      extent: extent,
      pageCount: 1,
    );
    expect(first, 0);
    expect(last, 0);
  });
}
