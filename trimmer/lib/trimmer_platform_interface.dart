import 'package:plugin_platform_interface/plugin_platform_interface.dart';

import 'trimmer_method_channel.dart';

abstract class TrimmerPlatform extends PlatformInterface {
  /// Constructs a TrimmerPlatform.
  TrimmerPlatform() : super(token: _token);

  static final Object _token = Object();

  static TrimmerPlatform _instance = MethodChannelTrimmer();

  /// The default instance of [TrimmerPlatform] to use.
  ///
  /// Defaults to [MethodChannelTrimmer].
  static TrimmerPlatform get instance => _instance;

  /// Platform-specific implementations should set this with their own
  /// platform-specific class that extends [TrimmerPlatform] when
  /// they register themselves.
  static set instance(TrimmerPlatform instance) {
    PlatformInterface.verifyToken(instance, _token);
    _instance = instance;
  }

  Future<String?> getPlatformVersion() {
    throw UnimplementedError('platformVersion() has not been implemented.');
  }
}
