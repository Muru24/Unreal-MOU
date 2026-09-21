#include "Session/Session.h"

#include <atomic>
#include <cstdlib>
#include <iostream>
#include <thread>

static void Check(bool Condition, const char* Message)
{
	if (!Condition) { std::cerr << Message << '\n'; std::exit(1); }
}

int main()
{
	MOU::SessionManager Sessions;
	const auto First = Sessions.Add(MOU::kInvalidSocket);
	const auto Second = Sessions.Add(MOU::kInvalidSocket);
	std::atomic<bool> Start{false};
	bool FirstWon = false;
	bool SecondWon = false;

	std::thread A([&]
	{
		while (!Start.load()) { std::this_thread::yield(); }
		FirstWon = Sessions.TryClaimAccount(First, 42, "first", 0);
	});
	std::thread B([&]
	{
		while (!Start.load()) { std::this_thread::yield(); }
		SecondWon = Sessions.TryClaimAccount(Second, 42, "second", 0);
	});
	Start = true;
	A.join();
	B.join();
	Check(FirstWon != SecondWon, "concurrent login allowed both sessions or neither");

	const auto Winner = FirstWon ? First : Second;
	const auto Loser = FirstWon ? Second : First;
	Check(!Sessions.TryClaimAccount(Winner, 43, "other", 0), "same session changed account");
	Sessions.Remove(Loser);
	const auto Third = Sessions.Add(MOU::kInvalidSocket);
	Check(!Sessions.TryClaimAccount(Third, 42, "third", 0), "rejected session freed account");
	Sessions.Remove(Winner);
	Check(Sessions.TryClaimAccount(Third, 42, "third", 0), "disconnect did not free account");
	Sessions.Remove(Third);
	std::cout << "SessionAuthTest passed\n";
}
