import 'package:flutter/src/services/platform_channel.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:trimmer/trimmer.dart';
import 'package:plugin_platform_interface/plugin_platform_interface.dart';

class MockTrimmerPlatform with MockPlatformInterfaceMixin implements TrimmerPlatform {
  @override
  Future<String?> getPlatformVersion() => Future.value('42');

  @override
  // TODO: implement channel
  MethodChannel get channel => throw UnimplementedError();
}

void main() {
  final TrimmerPlatform initialPlatform = TrimmerPlatform.instance;

  test('$MethodChannelTrimmer is the default instance', () {
    expect(initialPlatform, isInstanceOf<MethodChannelTrimmer>());
  });

  test('getPlatformVersion', () async {
    Trimmer trimmerPlugin = Trimmer();
    MockTrimmerPlatform fakePlatform = MockTrimmerPlatform();
    TrimmerPlatform.instance = fakePlatform;

    expect(await trimmerPlugin.getPlatformVersion(), '42');
  });
}
