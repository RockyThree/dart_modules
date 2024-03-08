# 作用

作为 washer 的子模块集合, 以 git 子模块形式使用

# 如何为 lib 添加平台支持

```bash
flutter create --platforms=windows,macos,linux .
flutter create --platforms=windows .

flutter create -t plugin --platforms windows .
```

# 插件与底层通讯

```dart
// dart 主动触发,等待回值
final platformChannel = MethodChannel('channel_name');
final result = await platformChannel.invokeMethod('method_name', arguments);

// 被动触发dart侧listen
final eventChannel = EventChannel('channel_name');
final eventStream = eventChannel.receiveBroadcastStream();
eventStream.listen((event) {
    // Handle events or data received from the platform
});

// 双向对象 BasicMessageChannel
static const _basicMessageChannel = BasicMessageChannel<dynamic>('platformImageDemo', StandardMessageCodec(),);
// Send a message to request an image from the platform using the BasicMessageChannel.
final reply = await _basicMessageChannel.send('getImage') as Uint8List?;

// 双向对象: String messages
// Dart side
const channel = BasicMessageChannel<String>('foo', StringCodec());
// Send message to platform and receive reply.
final String reply = await channel.send('Hello, world');
print(reply);
// Receive messages from platform and send replies.
channel.setMessageHandler((String message) async {
  print('Received: $message');
  return 'Hi from Dart';
});

```

* StringCodec使用 UTF-8 对字符串进行编码。BasicMessageChannel<String>
* BinaryCodec通过在字节缓冲区上实现身份映射，该编解码器允许您在不需要编码/解码的情况下享受通道对象的便利。 BasicMessageChannel<ByteData>
* JSONMessageCodec处理“类似 JSON”的值（字符串、数字、布尔值、null、此类值的列表以及此类值的字符串键控映射）BasicMessageChannel<dynamic>
* StandardMessageCodec处理比 JSON 编解码器更通用的值
