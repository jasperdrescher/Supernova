#pragma once

#include <type_traits>

template<typename Enum>
struct BitmaskOperators
{
	static constexpr bool mIsEnabled = false;
};

template<typename Enum>
inline constexpr auto operator|(Enum aLhs, Enum aRhs) -> std::enable_if_t<BitmaskOperators<Enum>::mIsEnabled, Enum>
{
	using underlying = std::underlying_type_t<Enum>;
	return static_cast<Enum>(static_cast<underlying>(aLhs) | static_cast<underlying>(aRhs));
}

template<typename Enum>
inline constexpr auto operator&(Enum aLhs, Enum aRhs) -> std::enable_if_t<BitmaskOperators<Enum>::mIsEnabled, Enum>
{
	using underlying = std::underlying_type_t<Enum>;
	return static_cast<Enum>(static_cast<underlying>(aLhs) & static_cast<underlying>(aRhs));
}

template<typename Enum>
inline constexpr auto operator|=(Enum& aLhs, Enum aRhs) -> std::enable_if_t<BitmaskOperators<Enum>::mIsEnabled, Enum&>
{
	aLhs = aLhs | aRhs;
	return aLhs;
}

template<typename Enum>
inline constexpr auto operator&=(Enum& aLhs, Enum aRhs) -> std::enable_if_t<BitmaskOperators<Enum>::mIsEnabled, Enum&>
{
	aLhs = aLhs & aRhs;
	return aLhs;
}

template<typename Enum>
inline constexpr bool HasFlag(Enum aFlags, Enum aFlag)
{
	static_assert(std::is_enum_v<Enum>, "Given type is not an enum");

	using underlying = std::underlying_type_t<Enum>;
	return (static_cast<underlying>(aFlags) & static_cast<underlying>(aFlag)) == static_cast<underlying>(aFlag);
}
