#include <chrono>
#include <functional>

/****
 *  封装异步操作并返回其执行时间
    auto duration = executeAsyncOperationAndGetDuration([&]() {

    });
    std::cout << "时间消耗: " << duration.count() << " milliseconds" << std::endl;
 *
*/
template <typename Func>
auto executeAsyncOperationAndGetDuration(Func func)
{
    auto startTime = std::chrono::steady_clock::now();                                 // 异步操作开始时间
    func();                                                                            // 执行异步操作
    auto endTime = std::chrono::steady_clock::now();                                   // 异步操作结束时间
    return std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime); // 返回操作消耗的时间
}