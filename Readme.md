# 作用
作为washer的子模块集合, 以git子模块形式使用

# 如何为lib添加平台支持
```bash
flutter create --platforms=windows,macos,linux .
flutter create --platforms=windows .

flutter create -t plugin --platforms windows .
```