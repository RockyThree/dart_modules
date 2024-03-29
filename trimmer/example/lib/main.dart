import 'dart:io';

import 'package:flutter/material.dart';
import 'dart:async';

import 'package:flutter/services.dart';
import 'package:flutter/widgets.dart';
import 'package:path_provider/path_provider.dart';
import 'package:trimmer/trimmer.dart';
import 'package:path/path.dart' as p;

void main() {
  runApp(const MyApp());
}

class MyApp extends StatefulWidget {
  const MyApp({super.key});

  @override
  State<MyApp> createState() => _MyAppState();
}

Future<File> copyAssetToTemporaryFile(String assetPath) async {
  final byteData = await rootBundle.load(assetPath);
  final file = File('${(await getTemporaryDirectory()).path}/temp_video.mp4');
  await file.writeAsBytes(byteData.buffer.asUint8List());
  print(file.path);
  return file;
}

class _MyAppState extends State<MyApp> {
  String _platformVersion = 'Unknown';
  final _trimmerPlugin = Trimmer();

  List<String> imgList = [];
  late File videoFile;
  late String tmpPath;

  @override
  void initState() {
    super.initState();
    initPlatformState();

    () async {
      videoFile = await copyAssetToTemporaryFile('assets/Butterfly-209.mp4');
      tmpPath = (await getTemporaryDirectory()).path;
      // VideoThumbnail.saveFrameToFile(
      //   videoPath: videoFile.path,
      //   frameIndex: 2,
      //   frameSavedDirPath: p.join((await getTemporaryDirectory()).path, '45345354345.png'),
      // );
      // exit(0);
    }();
  }

  // Platform messages are asynchronous, so we initialize in an async method.
  Future<void> initPlatformState() async {
    String platformVersion;
    // Platform messages may fail, so we use a try/catch PlatformException.
    // We also handle the message potentially returning null.
    try {
      platformVersion = await _trimmerPlugin.getPlatformVersion() ?? 'Unknown platform version';
    } on PlatformException {
      platformVersion = 'Failed to get platform version.';
    }

    // If the widget was removed from the tree while the asynchronous platform
    // message was in flight, we want to discard the reply rather than calling
    // setState to update our non-existent appearance.
    if (!mounted) return;

    setState(() {
      _platformVersion = platformVersion;
    });
  }

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      home: Scaffold(
        appBar: AppBar(
          title: const Text('Plugin example app'),
        ),
        body: Center(
          child: Column(
            children: [
              Text('Running on: $_platformVersion\n'),
              TextButton(
                onPressed: () {
                  var indexFrame = imgList.length;
                  var pngPath = p.join(tmpPath, 'test${indexFrame}.png');
                  VideoThumbnail.saveFrameToFile(
                    videoPath: videoFile.path,
                    frameIndex: indexFrame,
                    frameSavedDirPath: pngPath,
                  ).then((value) {
                    imgList.add(pngPath);
                    setState(() {});
                  });
                },
                child: Text('切帧 ${imgList.length}'),
              ),
              Expanded(
                child: GridView(
                  gridDelegate: SliverGridDelegateWithFixedCrossAxisCount(
                    crossAxisCount: 5, // number of items in each row
                    mainAxisSpacing: 1.0, // spacing between rows
                    crossAxisSpacing: 1.0, // spacing between columns
                  ),
                  children: imgList.reversed
                      .map((e) => Image.file(
                            File(e),
                            height: 120,
                            fit: BoxFit.contain,
                            alignment: AlignmentDirectional.center,
                          ))
                      .toList(),
                ),
              )
            ],
          ),
        ),
      ),
    );
  }
}
