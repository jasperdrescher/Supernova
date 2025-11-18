include(FetchContent)

FetchContent_Declare(
	EnTT
	GIT_REPOSITORY https://github.com/skypjack/entt.git
	GIT_TAG v3.15.0
	GIT_SHALLOW TRUE)

FetchContent_MakeAvailable(EnTT)
