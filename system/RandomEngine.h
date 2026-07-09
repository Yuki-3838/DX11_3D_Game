// RandomEngine.h
#pragma once
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <limits>
#include <random>
#include <span>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

/**
 * @file   RandomEngine.h
 * @brief  乱数ユーティリティ：決定論/エントロピー初期化・サブストリーム・分布・シャッフル等を提供するヘッダオンリークラス
 * @author 
 * @version 1.0
 *
 * @details
 * - 乱数エンジンに std::mt19937_64（メルセンヌツイスター64）を採用
 * - 真のエントロピー（std::random_device）＋時刻等から安全に初期化
 * - 固定シードでの決定論的な再現も可能
 * - 名前付きサブストリーム（"enemyAI" や "loot" など）を簡単に分離
 * - 各種分布：一様(整数/実数)、ベルヌーイ、正規、離散（重み付きインデックス）
 * - ユーティリティ：シャッフル、サンプリング、選択(choice)
 * - スレッドローカルのデフォルトエンジンを提供（並列対応）
 *
 * 使用例:
 *   auto& rng = RandomEngine::tls();                 // スレッドローカル RNG
 *   int x = rng.uniformInt(1, 6);                    // 1..6
 *   double y = rng.uniformReal(0.0, 1.0);            // [0,1)
 *   bool b = rng.bernoulli(0.25);                    // 25%
 *   double n = rng.normal(0.0, 1.0);                 // N(0,1)
 *   size_t i = rng.weightedIndex({0.1, 0.2, 0.7});   // 0/1/2 を重みに応じて選ぶ
 *   auto lootRng = rng.stream("loot");               // 名前で独立ストリーム
 *   std::vector<int> a{1,2,3,4,5}; rng.shuffle(a);   // シャッフル
 */

class RandomEngine {
public:
    using engine_type = std::mt19937_64;

    /// @brief エントロピーから初期化
    RandomEngine() { reseedFromEntropy(); }

    /// @brief 固定シードで決定論的に初期化
    explicit RandomEngine(uint64_t seed) { reseed(seed); }

    /// @brief 現在の代表シード値（ログ/再現のメモ用）
    [[nodiscard]] uint64_t seedValue() const noexcept { return seed_value_; }

    /// @brief 固定シードにリシード（決定論）
    void reseed(uint64_t seed) {
        seed_value_ = seed;
        eng_.seed(seed_value_);
    }

    /// @brief 真のエントロピーからリシード（非決定論）
    void reseedFromEntropy() {
        auto [mix, representative] = makeSeedSeqFromEntropy(); // ← 配列を受け取る
        seed_value_ = representative;
        std::seed_seq seq(mix.begin(), mix.end());             // ← ここで組み立て
        eng_.seed(seq);
    }

    /// @brief 生の64bit乱数（必要なら）
    uint64_t u64() { return eng_(); }

    /// @brief [0,1) の一様実数（倍精度）
    double uniform01() {
        // 53bit精度で [0,1) を生成
        return std::generate_canonical<double, 53>(eng_);
    }

    /// @brief (0,1) の一様実数（端点除外）
    double uniform01Open() {
        // 0 と 1 を避ける工夫
        double x;
        do { x = uniform01(); } while (x == 0.0 || x == 1.0);
        return x;
    }

    /// @brief [lo, hi] の一様整数（両端含む）
    template <class Int>
    Int uniformInt(Int lo, Int hi) {
        static_assert(std::is_integral_v<Int>, "uniformInt requires integral type");
        std::uniform_int_distribution<Int> dist(lo, hi);
        return dist(eng_);
    }

    /// @brief [lo, hi) の一様実数（半開区間）
    double uniformReal(double lo, double hi) {
        std::uniform_real_distribution<double> dist(lo, std::nextafter(hi, std::numeric_limits<double>::lowest()));
        return dist(eng_);
    }

    /// @brief p の確率で true（ベルヌーイ）
    bool bernoulli(double p) {
        std::bernoulli_distribution dist(p);
        return dist(eng_);
    }

    /// @brief 正規分布 N(mean, stddev^2)
    double normal(double mean = 0.0, double stddev = 1.0) {
        std::normal_distribution<double> dist(mean, stddev);
        return dist(eng_);
    }

    /// @brief 重み配列（非負）からインデックスを1つ返す（離散分布）
    size_t weightedIndex(std::span<const double> weights) {
        std::discrete_distribution<size_t> dist(weights.begin(), weights.end());
        return dist(eng_);
    }
    size_t weightedIndex(const std::vector<double>& weights) {
        return weightedIndex(std::span<const double>(weights.data(), weights.size()));
    }

    /// @brief 範囲をシャッフル（コンテナ／イテレータ両対応）
    template <class It>
    void shuffle(It first, It last) {
        std::shuffle(first, last, eng_);
    }
    template <class Container>
    void shuffle(Container& c) { shuffle(c.begin(), c.end()); }

    /// @brief 範囲から n 個を無作為抽出（非破壊）
    template <class It, class OutIt>
    void sample(It first, It last, OutIt out, size_t n) {
        std::sample(first, last, out, n, eng_);
    }

    /// @brief コンテナからランダムに1要素を返す（要素数>0前提）
    template <class Container>
    decltype(auto) choice(const Container& c) {
        size_t idx = uniformInt<size_t>(0, c.size() - 1);
        return c[idx];
    }

    /// @brief 名前から独立ストリームを生成（同じ seedValue + 同じ名前 → 同じ系列）
    RandomEngine stream(std::string_view tag) const {
        uint64_t tagHash = fnv1a64(tag);
        uint64_t subSeed = splitmix64(seed_value_ ^ tagHash);
        return RandomEngine(subSeed);
    }

    /// @brief スレッドローカルな既定エンジン（初期化はエントロピー）
    static RandomEngine& tls() {
        thread_local RandomEngine s_tls;
        return s_tls;
    }

    /// @brief 基礎エンジンへのアクセス（高度な用途）
    engine_type& engine() noexcept { return eng_; }
    const engine_type& engine() const noexcept { return eng_; }

private:
    engine_type eng_{};
    uint64_t    seed_value_{ 0 };

    // ---- 低レベルユーティリティ ----

    static uint64_t splitmix64(uint64_t x) {
        x += 0x9E3779B97F4A7C15ull;
        x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ull;
        x = (x ^ (x >> 27)) * 0x94D049BB133111EBull;
        return x ^ (x >> 31);
    }

    static uint64_t fnv1a64(std::string_view s) {
        uint64_t h = 1469598103934665603ull;
        for (unsigned char c : s) {
            h ^= c;
            h *= 1099511628211ull;
        }
        return h;
    }

    // std::random_device が実装依存で弱い環境でも安全側に寄せるため、
    // rd() 複数値 + 時刻/アドレス由来ビットを seed_seq に混ぜる
    static std::pair<std::array<uint32_t, 16 + 4>, uint64_t> makeSeedSeqFromEntropy() {
        std::random_device rd;
        constexpr size_t kWords = 16;
        std::array<uint32_t, kWords> seed_data{};
        for (size_t i = 0; i < kWords; ++i) seed_data[i] = rd();

        auto now = static_cast<uint64_t>(
            std::chrono::high_resolution_clock::now().time_since_epoch().count());

        uint64_t addr = splitmix64(reinterpret_cast<uintptr_t>(&seed_data) ^
            reinterpret_cast<uintptr_t>(&now));

        static uint64_t counter = 0;
        uint64_t cnt = ++counter;

        uint64_t representative = 0;
        for (auto w : seed_data) representative = splitmix64(representative ^ w);
        representative ^= splitmix64(now);
        representative ^= splitmix64(addr);
        representative ^= splitmix64(cnt);

        std::array<uint32_t, kWords + 4> mix{};
        std::copy(seed_data.begin(), seed_data.end(), mix.begin());
        mix[kWords + 0] = static_cast<uint32_t>(representative);
        mix[kWords + 1] = static_cast<uint32_t>(representative >> 32);
        mix[kWords + 2] = static_cast<uint32_t>(now);
        mix[kWords + 3] = static_cast<uint32_t>(now >> 32);

        return { mix, representative };  // ← seed_seq を返さない
    }
};
