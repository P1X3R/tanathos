# ♟️ Tanathos: The Chess Engine That Probably Shouldn't Exist

> "I don't always lose to Stockfish, but when I do, I blame the eval function."

## 🌟 Features

* 🚀 Blazing Fast Move Generation (if you consider 2 nodes/sec "fast")
* 🤖 Advanced AI (literally just `if (piece == 'Q') score += 900`)
* 📡 UCI Protocol Support (responds to go home with "I wish")
* 🧠 100% No Neural Nets (because I still don't know what a tensor is)
* 💀 Written in C++ because I hate myself.

## 🧩 Why "Tanathos"?

* ✍️ Misspelled greek name as my previous chess-related project (Sysifus).
* 🎯 MIT Application Bait™ (now with 200% more bitboards).

## ⚡ Quickstart

1. **Compile** (if you dare):

```bash
xmake && xmake run
```

2. There you have, deal with it 😤

3. **Blame the eval**

```cpp
int evaluate(ChessBoard &board) {
    return rand() % 1000; // "Stochastic optimization"
}
```

## 🤝 Contributing

PRs welcome! (Disclaimer: "Welcome" ≠ "Merged")

* 🐛 **Bugfixes:** Must include meme in commit message.
* ✨ **Features:** Must swear in code comments.
* 📚 **Docs:** Lol.

## 📜 License

This project is licensed under the MIT License - see the LICENSE file for details.
But we're working in the "Don't Be a Jerk" Public License, that in short is:

> whatever, but don’t sue me when Stockfish mocks your life choices. 🙃

## 🎓 MIT Application Bonus Round

> "Tanathos demonstrates my ability to ~~fail spectacularly~~ innovate under pressure. By replacing traditional evaluation with vibes, I achieved a 0% win rate against Leela Chess Zero—a groundbreaking result." 🚫🏆
