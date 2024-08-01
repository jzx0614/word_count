- install boost into mingw64

- idea
  - Load Balance
    - 使用一個 queue 動態分配
    - queue model
    - 每個 thread 一個 queue, 根據 queue 的長度分配
    
    - i/o bound 檔案多
    - cpu bound 檔案大

- clinet
  - 使用 fs::directory_iterator 來掃目錄
  - 使用 fs::last_write_time 取得最後修改時間
  - fs::file_time_type 與 time_t 的比較大小
  - 使用 boost::asio 下的 tcp client
  - 送出 vector.size() ( string + "\n" ) * size

- server
  - 使用 boost::asio 下的 tcp server
  - 使用一個 queue 將每個 path 切成一個個小任務
  - 每個 thread 從 queue 中，動態拿取 task 做平行化
  - class ParelledlWordCount 管理 thread, 以及回收 thread 的結果做 merge
  - class CalculateWordCount 為計算 word_count, 可疊加多個 file path
  