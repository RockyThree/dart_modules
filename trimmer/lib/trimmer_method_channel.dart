import 'package:flutter/foundation.dart';
import 'package:flutter/services.dart';

import 'trimmer_platform_interface.dart';

/// An implementation of [TrimmerPlatform] that uses method channels.
class MethodChannelTrimmer extends TrimmerPlatform {
  /// The method channel used to interact with the native platform.
  @visibleForTesting
  final methodChannel = const MethodChannel('trimmer');

  @override
  Future<String?> getPlatformVersion() async {
    final version = await methodChannel.invokeMethod<String>('getPlatformVersion');
    return version;
  }
}
