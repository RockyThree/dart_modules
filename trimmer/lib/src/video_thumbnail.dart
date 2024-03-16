import 'dart:async';
import 'dart:typed_data';

import 'package:flutter/services.dart';

import 'trimmer_platform_interface.dart';

enum ImageFormat { JPEG, PNG, WEBP }

class VideoThumbnail {
  static Future<String?> thumbnailFile(
      {required String video,
      Map<String, String>? headers,
      String? thumbnailPath,
      ImageFormat imageFormat = ImageFormat.PNG,
      int maxHeight = 0,
      int maxWidth = 0,
      int timeMs = 0,
      int quality = 10}) async {
    assert(video.isNotEmpty);
    if (video.isEmpty) return null;
    final reqMap = <String, dynamic>{
      'video': video,
      'headers': headers,
      'path': thumbnailPath,
      'format': imageFormat.index,
      'maxh': maxHeight,
      'maxw': maxWidth,
      'timeMs': timeMs,
      'quality': quality
    };
    return await TrimmerPlatform.instance.channel.invokeMethod('file', reqMap);
  }

  static Future<Uint8List?> thumbnailPNGData({
    required String videoPath,
    int frameIndex = 0,
  }) async {
    assert(videoPath.isNotEmpty);
    final reqMap = <String, dynamic>{
      'videoPath': videoPath,
      'frameIndex': frameIndex,
    };
    var res = await TrimmerPlatform.instance.channel.invokeMethod('thumbnailPNGData', reqMap);
    return res;
  }

  static Future<int?> getTotalFrames({required String videoPath}) async {
    assert(videoPath.isNotEmpty);
    final reqMap = <String, dynamic>{
      'videoPath': videoPath,
    };
    return await TrimmerPlatform.instance.channel.invokeMethod('getTotalFrames', reqMap);
  }

  static Future<double?> getAvgFrameDuration({required String videoPath}) async {
    assert(videoPath.isNotEmpty);
    final reqMap = <String, dynamic>{
      'videoPath': videoPath,
    };
    return await TrimmerPlatform.instance.channel.invokeMethod('getAvgFrameDuration', reqMap);
  }

  static Future<int?> getDuration({required String videoPath}) async {
    assert(videoPath.isNotEmpty);
    final reqMap = <String, dynamic>{
      'videoPath': videoPath,
    };
    return await TrimmerPlatform.instance.channel.invokeMethod('getDuration', reqMap);
  }

  static Future<bool?> releaseVideoDecoder({required String videoPath}) async {
    assert(videoPath.isNotEmpty);
    final reqMap = <String, dynamic>{
      'videoPath': videoPath,
    };
    return await TrimmerPlatform.instance.channel.invokeMethod('releaseVideoDecoder', reqMap);
  }

  static Future<bool> saveFrameToFile({
    required String videoPath,
    required String frameSavedDirPath,
    int frameIndex = 0,
  }) async {
    assert(videoPath.isNotEmpty);
    assert(frameSavedDirPath.isNotEmpty);
    final reqMap = <String, dynamic>{
      'videoPath': videoPath,
      'frameIndex': frameIndex,
      'frameSavedDirPath': frameSavedDirPath,
    };
    return await TrimmerPlatform.instance.channel.invokeMethod('saveFrameToFile', reqMap);
  }
}
