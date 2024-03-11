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

  static Future<Uint8List?> thumbnailData({
    required String video,
    Map<String, String>? headers,
    ImageFormat imageFormat = ImageFormat.PNG,
    int maxHeight = 0,
    int maxWidth = 0,
    int timeMs = 0,
    int quality = 10,
  }) async {
    assert(video.isNotEmpty);
    final reqMap = <String, dynamic>{
      'video': video,
      'headers': headers,
      'format': imageFormat.index,
      'maxh': maxHeight,
      'maxw': maxWidth,
      'timeMs': timeMs,
      'quality': quality,
    };
    return await TrimmerPlatform.instance.channel.invokeMethod('data', reqMap);
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
