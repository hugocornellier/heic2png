import 'package:flutter_test/flutter_test.dart';

import 'package:heic_native_example/main.dart';

void main() {
  testWidgets('App renders initial UI', (WidgetTester tester) async {
    await tester.pumpWidget(const MyApp());

    expect(find.text('Status: Idle'), findsOneWidget);
    expect(find.text('Convert to file'), findsOneWidget);
    expect(find.text('Convert to bytes'), findsOneWidget);
  });
}
