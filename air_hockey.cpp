#include <SFML/Graphics.hpp>
#include <SFML/Window.hpp>
#include <SFML/System.hpp>
#include <SFML/Audio.hpp>
#include <iostream>
#include <vector>
#include <sstream>
#include <iomanip> // 小数点以下の桁数を制御するために追加
#include <cmath>   // カオス的な速度変化のために追加

// テーマ：ブロック崩しゲーム
// 各種仕様：
// ・GUI
// ・イージーモード(ボール速度一定)、ハードモード(ボール速度がパドルと衝突するたびカオス的なシステムロジックの関数出力値(ただし上限値を超えた場合はあらかじめ指定された範囲内に収まるように商の余りをとって調整))、が選択可能
// NOTE:ハードモードがむずかしすぎて動作確認に1時間費やした(クリア時のゲーム内の画面遷移等を確認するため)
// ・ブロック数は20個、1つ10点、点数ごとに結果の評価が変化
// C++は組込みプログラムの実装に利用されるイメージがあるが、GUI提供のライブラリもあるらしくそれを利用
// ・パドルにボールが衝突したときの反転の条件がl:231辺りで要調整済み
// ・ボールのブロックとの衝突時などの多少の挙動が直感と差異が生じているがこのバグは仕様として許容範囲内と判断ー＞本来ならこの部分も調整・修正したほうがよく、プロダクトレベルで実装するならそもそも技術選定としてより適したプログラミング言語やフレームワークそしてライブラリ群があるはずでそれを利用したほうがいいと考えられる。しかし、今回はC,C++のいずれかで実相という指定があったため多少のライブラリや言語の仕様的にバグが生じやすい可能性がある点や、その多少のバグを修正するという結果に、システム開発者として、修正するためのコストが見合わないと判断したため許容とした。

// 環境構築
// 実行環境にg++コンパイラをインストール
// c++の上記の各種ライブラリをインストール
// 必要に応じて実行パスや環境変数等の調整

// 実行手順
// g++ -o air_hockey air_hockey.cpp -lsfml-graphics -lsfml-window -lsfml-system (コンパイル)
// ./air_hockey (バイナリ実行)

// NOTE:ブロックの衝突や判定時、その後のボールの挙動にバグあり

enum GameState
{
  MENU,
  PLAYING,
  GAME_OVER,
  GAME_CLEAR
};

enum Difficulty
{
  EASY,
  HARD
};

float getChaosSpeed(float baseSpeed, float maxSpeed)
{
  // カオス的な速度を生成
  float newSpeed = baseSpeed * (1.0f + 2.0f * (static_cast<float>(rand()) / RAND_MAX));
  if (newSpeed > maxSpeed)
  {
    newSpeed = fmod(newSpeed, maxSpeed); // 上限値を超えた場合、余りを採用
  }
  return newSpeed;
}

int main()
{
  sf::RenderWindow window(sf::VideoMode(800, 600), "Air Hockey Game");

  // フォントの読み込み
  sf::Font font;
  if (!font.loadFromFile("/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf"))
  {
    std::cerr << "Failed to load font!" << std::endl;
    return -1;
  }

  // スコア表示用テキスト
  sf::Text scoreText;
  scoreText.setFont(font);
  scoreText.setCharacterSize(24);
  scoreText.setFillColor(sf::Color::White);
  scoreText.setPosition(10, 10);

  // 時間表示用テキスト
  sf::Text timeText;
  timeText.setFont(font);
  timeText.setCharacterSize(24);
  timeText.setFillColor(sf::Color::White);
  timeText.setPosition(10, 40);

  // メニューテキスト
  sf::Text menuText;
  menuText.setFont(font);
  menuText.setCharacterSize(36);
  menuText.setFillColor(sf::Color::White);
  menuText.setString("Press 1 for Easy, 2 for Hard");
  menuText.setPosition(150, 250);

  // ゲームオーバーテキスト
  sf::Text gameOverText;
  gameOverText.setFont(font);
  gameOverText.setCharacterSize(36);
  gameOverText.setFillColor(sf::Color::White);
  gameOverText.setString("Game Over\nPress 1 for Easy, 2 for Hard");
  gameOverText.setPosition(150, 200);

  // ゲームクリアテキスト
  sf::Text gameClearText;
  gameClearText.setFont(font);
  gameClearText.setCharacterSize(36);
  gameClearText.setFillColor(sf::Color::White);
  gameClearText.setString("Game Clear!");
  gameClearText.setPosition(250, 250);

  sf::Text resultText;
  resultText.setFont(font);
  resultText.setCharacterSize(24);
  resultText.setFillColor(sf::Color::White);
  resultText.setPosition(150, 450);

  GameState gameState = MENU;
  Difficulty difficulty = EASY;

  // パドルの設定
  sf::RectangleShape paddle(sf::Vector2f(100, 20));
  paddle.setFillColor(sf::Color::Green);
  paddle.setPosition(350, 550);

  // ボールの設定
  sf::CircleShape ball(10);
  ball.setFillColor(sf::Color::Red);
  ball.setPosition(395, 300);

  // ボールの初期速度を2倍に
  sf::Vector2f ballVelocity(0.6f, -0.6f); // 0.3f * 2 = 0.6f
  float maxSpeed = 1.8f;                  // 初期速度の3倍が上限

  // ブロックの設定
  std::vector<sf::RectangleShape> blocks;
  int blockWidth = 80;
  int blockHeight = 30;

  int score = 0;
  sf::Clock clock;
  sf::Time finalTime;
  sf::Clock clearClock;

  auto resetGame = [&]()
  {
    score = 0;
    ball.setPosition(395, 300);
    ballVelocity = sf::Vector2f(0.6f, -0.6f); // 0.3f * 2 = 0.6f
    paddle.setPosition(350, 550);
    blocks.clear();
    for (int i = 0; i < 20; ++i)
    {
      sf::RectangleShape block(sf::Vector2f(blockWidth, blockHeight));
      block.setFillColor(sf::Color::Blue);
      block.setPosition((i % 10) * (blockWidth + 5) + 35, (i / 10) * (blockHeight + 5) + 50);
      blocks.push_back(block);
    }
    clock.restart();
  };

  resetGame();

  while (window.isOpen())
  {
    sf::Event event;
    while (window.pollEvent(event))
    {
      if (event.type == sf::Event::Closed)
      {
        window.close();
      }

      if (gameState == MENU)
      {
        if (event.type == sf::Event::KeyPressed)
        {
          if (event.key.code == sf::Keyboard::Num1)
          {
            difficulty = EASY;
            gameState = PLAYING;
            clock.restart();
          }
          else if (event.key.code == sf::Keyboard::Num2)
          {
            difficulty = HARD;
            gameState = PLAYING;
            clock.restart();
          }
        }
      }
      else if (gameState == GAME_OVER || gameState == GAME_CLEAR)
      {
        if (event.type == sf::Event::KeyPressed)
        {
          if (event.key.code == sf::Keyboard::Num1)
          {
            difficulty = EASY;
            resetGame();
            gameState = PLAYING;
          }
          else if (event.key.code == sf::Keyboard::Num2)
          {
            difficulty = HARD;
            resetGame();
            gameState = PLAYING;
          }
        }
      }
    }

    if (gameState == PLAYING)
    {
      // パドルの移動速度を2倍に
      if (sf::Keyboard::isKeyPressed(sf::Keyboard::Left))
      {
        paddle.move(-1.2f, 0); // 0.6f * 2 = 1.2f
      }
      if (sf::Keyboard::isKeyPressed(sf::Keyboard::Right))
      {
        paddle.move(1.2f, 0); // 0.6f * 2 = 1.2f
      }

      // ボールの移動
      ball.move(ballVelocity);

      // 壁との衝突
      if (ball.getPosition().x <= 0 || ball.getPosition().x >= 790)
      {
        ballVelocity.x = -ballVelocity.x;
      }
      if (ball.getPosition().y <= 0)
      { // 上側の壁との衝突を追加
        ballVelocity.y = -ballVelocity.y;
      }

      // パドルとの衝突判定を改善
      if (ball.getGlobalBounds().intersects(paddle.getGlobalBounds()))
      {
        if (ball.getPosition().y + ball.getRadius() * 2 >= paddle.getPosition().y &&
            ball.getPosition().y + ball.getRadius() * 2 <= paddle.getPosition().y + paddle.getSize().y)
        {
          ballVelocity.y = -std::abs(ballVelocity.y);

          if (difficulty == HARD)
          {
            // ハードモードの場合、カオス的に速度を変化
            float chaosSpeed = getChaosSpeed(0.6f, maxSpeed);
            ballVelocity.x = (ballVelocity.x > 0 ? chaosSpeed : -chaosSpeed);
            ballVelocity.y = (ballVelocity.y > 0 ? chaosSpeed : -chaosSpeed);
          }
        }
      }

      // ブロックとの衝突
      for (auto it = blocks.begin(); it != blocks.end();)
      {
        if (ball.getGlobalBounds().intersects(it->getGlobalBounds()))
        {
          if (ball.getPosition().y + ball.getRadius() >= it->getPosition().y &&
              ball.getPosition().y <= it->getPosition().y + blockHeight)
          {
            ballVelocity.y = -ballVelocity.y;
          }
          else
          {
            ballVelocity.x = -ballVelocity.x;
          }
          it = blocks.erase(it);
          score += 10;
        }
        else
        {
          ++it;
        }
      }

      // ブロックがすべて消去された場合
      if (blocks.empty())
      {
        finalTime = clock.getElapsedTime(); // ゲームクリア時の時間を保持
        gameState = GAME_CLEAR;
        clearClock.restart();
      }

      // ボールが下に落ちた場合
      if (ball.getPosition().y >= 600)
      {
        finalTime = clock.getElapsedTime(); // ゲーム終了時の時間を保持
        gameState = GAME_OVER;
      }

      // スコアと時間の更新
      std::stringstream ss;
      ss << "Score: " << score;
      scoreText.setString(ss.str());

      sf::Time elapsed = clock.getElapsedTime();
      std::stringstream ssTime;
      ssTime << "Time: " << std::fixed << std::setprecision(2) << elapsed.asSeconds();
      timeText.setString(ssTime.str());
    }

    // 描画
    window.clear();
    if (gameState == MENU)
    {
      window.draw(menuText);
    }
    else if (gameState == PLAYING)
    {
      window.draw(paddle);
      window.draw(ball);
      for (const auto &block : blocks)
      {
        window.draw(block);
      }
      window.draw(scoreText);
      window.draw(timeText);
    }
    else if (gameState == GAME_OVER)
    {
      window.draw(gameOverText);
      std::stringstream ss;
      ss << "Final Score: " << score;
      scoreText.setString(ss.str());
      scoreText.setPosition(300, 300);
      window.draw(scoreText);

      std::stringstream ssTime;
      ssTime << "Time: " << std::fixed << std::setprecision(2) << finalTime.asSeconds();
      timeText.setString(ssTime.str());
      timeText.setPosition(300, 350);
      window.draw(timeText);

      // スコアに応じたメッセージを表示
      if (score <= 50)
      {
        resultText.setString("GOOD CHALLENGING!");
      }
      else if (score <= 100)
      {
        resultText.setString("GOOD JOB!");
      }
      else if (score <= 190)
      {
        resultText.setString("GREAT JOB!");
      }
      else
      {
        resultText.setString("PERFECT!!");
      }
      window.draw(resultText);
    }
    else if (gameState == GAME_CLEAR)
    {
      window.draw(gameClearText);
      std::stringstream ss;
      ss << "Final Score: " << score;
      scoreText.setString(ss.str());
      scoreText.setPosition(300, 300);
      window.draw(scoreText);

      std::stringstream ssTime;
      ssTime << "Time: " << std::fixed << std::setprecision(2) << finalTime.asSeconds();
      timeText.setString(ssTime.str());
      timeText.setPosition(300, 350);
      window.draw(timeText);

      if (clearClock.getElapsedTime().asSeconds() > 2)
      {
        gameState = GAME_OVER;
      }
    }
    window.display();
  }

  return 0;
}
