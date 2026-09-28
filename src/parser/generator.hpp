#pragma once

#include <functional>
#include <map>
#include <optional>

namespace z
{
  namespace core
  {

    /// Custom iterator for generators to allow for range-based for loops.
    template <typename T, typename S>
    class generatorIter
    {
      std::function<const std::optional<T>(S &)> lambda;
      S state;
      std::optional<T> current_yield;

    public:
      /**
       * @brief Constructor.
       * @param lambda The generator function.
       * @param state The state data that may be mutated by the generator function.
       * @param dummy If true, do not generate data. This is here just so range-based loop syntax will work.
       */
      explicit generatorIter(std::function<const std::optional<T>(S &)> lambda, const S &state, bool dummy = false) : lambda(lambda), state(state), current_yield(T())
      {
        if (!dummy)
        {
          ++(*this); // Load the first value
        }
      }

      /**
       * @brief Get the current value from the generator.
       * @return The last value that was generated.
       */
      const T &operator*() const
      {
        return current_yield.value();
      }

      /**
       * @brief Generate the next value.
       * @return This iterator after getting the next value from the generator.
       */
      generatorIter &operator++()
      {
        current_yield = lambda(state);
        return *this;
      }

      /**
       * @brief Check if the generator can get more data.
       * @return \b false if the generator is finished, \b true otherwise.
       */
      bool operator!=(const generatorIter &other) const
      {
        (void)other;
        return current_yield.has_value();
      }
    };

    /**
     * @brief An arbitrary generator for producing sequential results on-the-fly.
     *
     * This class encapsulates generator functionality with any state data the generator may require.
     * Some ideal uses for this class include looping over large sets of data that can't all be
     * loaded at runtime, or loading data in batches while still outputting only a small bit at a time.
     *
     * @tparam T The type of data that the generator returns.
     * @tparam S The state data for the generator.
     */
    template <typename T, typename S>
    class generator
    {
    protected:
      /// The state data of this generator.
      S state;

      /// The function that gets run every time an item is generated.
      std::function<const std::optional<T>(S &)> lambda;

      /// A specialized state for generator::enumerate()
      struct countedState
      {
        /// The current index.
        long count;
        /// The state data of the original generator.
        S state;
      };

    public:
      /**
       * @brief Constructor with an initial state.
       * @param initial The initial value of the generator's state.
       * @param lambda The generator function.
       */
      generator(const S &initial, std::function<const std::optional<T>(S &)> lambda) : state(initial), lambda(lambda) {}

      /**
       * @brief Begin iterator (start of the range)
       * @return An iterator that will give the first value in the generator.
       */
      generatorIter<T, S> begin() const noexcept
      {
        return generatorIter<T, S>(lambda, state);
      }

      /**
       * @brief End iterator (end of the range)
       * @return A dummy iterator indicating the end of the range.
       */
      generatorIter<T, S> end() const noexcept
      {
        return generatorIter<T, S>(lambda, state, true);
      }

      /**
       * @brief Get the next item from the generator.
       *
       * If there are no more items, the optional object will not contain anything.
       *
       * @return A std::optional object containing the next value, if any.
       */
      inline virtual std::optional<T> next()
      {
        return lambda(state);
      }

      /**
       * @brief Consume and discard all items from the generator, getting only the number of items generated.
       * @warning This function will consume the generator, and it will not be able to be used again.
       * @return The number of items that were generated.
       */
      long count()
      {
        long count = 0;
        for (auto _ : *this)
        {
          count++;
        }
        return count;
      }

      /**
       * @brief Concatenate all generator elements into an array.
       * @note This function will consume the generator, and it will not be able to be used again.
       * @return An array containing all elements from the generator.
       */
      std::vector<T> collect()
      {
        std::vector<T> result;
        for (auto i : *this)
        {
          result.push(i);
        }
        return result;
      }

      /**
       * @see count()
       * @copydoc count()
       */
      inline long consume()
      {
        return count();
      }

      /**
       * @brief Take a certain number of items from the generator.
       *
       * This function will take a certain number of items from the generator, or all items if there are fewer than the requested count.
       *
       * @param count The number of items to take.
       * @return An array containing the taken items.
       */
      std::vector<T> take(int count)
      {
        std::vector<T> result;
        result.increase(count);

        for (int i = 0; i < count; i++)
        {
          auto item = next();
          if (!item.has_value())
          {
            break;
          }
          result.push(item.value());
        }

        return result;
      }

      /**
       * @brief Applies a transformation function to each item that comes out of the generator.
       *
       * This function wraps the existing generator in another generator, effectively transforming
       * each item on-the-fly as it's generated.
       *
       * @tparam U The type of items that this new generator yields.
       * @param mapLambda A function that takes a constant reference to an element of type `T` and returns an element of type `U`.
       * @return A new generator that yields the transformed elements.
       */
      template <typename U>
      generator<U, S> map(std::function<U(const T &)> mapLambda) noexcept
      {
        auto lambda = this->lambda;

        return generator<U, S>(state, [lambda, mapLambda](S &state) -> std::optional<U>
                               {
			auto item = lambda(state);
			if (!item.has_value()) {
				return {};
			} else {
				return mapLambda(item.value());
			} });
      }

      /**
       * @brief Filters the generatred items based on a predicate and returns a new generator that yields only the items that satisfy the predicate.
       *
       * This function wraps the existing generator in another generator, and as each item is generated,
       * applies the given lambda function as a predicate, and only yields items that satisfy the predicate.
       *
       * @param filterLambda A function that takes a constant reference to an item of type `T` and returns a boolean indicating whether
       * the item should be yielded.
       * @return A new generator that yields only items that satisfy the predicate.
       */
      generator filter(std::function<bool(const T &)> filterLambda) noexcept
      {
        auto lambda = this->lambda;

        return generator(state, [lambda, filterLambda](S &state)
                         {
			auto val = lambda(state);
			while (val.has_value()) {
				if (filterLambda(val.value())) {
					return val;
				}
				val = lambda(state);
			}

			return val; });
      }

      /**
       * @brief Reduces the generator to a single value by applying a binary operation cumulatively to all yielded values.
       *
       * This function applies a binary operation (provided as a lambda) to combine all yielded items into a single value.
       * If the generator doesn't yield anything, the provided default value is returned.
       *
       * @note Especially for long lists of items, this is significantly more memory-efficient than calling collect() and then
       * reduce() on the resulting array, as an intermediate array does not need to be constructed.
       *
       * @param reduceLambda A function that takes two elements of type `T` and returns their combined result of type `T`.
       * @param defaultValue The value to return if the array is empty.
       * @return The result of the reduction operation.
       */
      T reduce(std::function<T(const T &, const T &)> reduceLambda, const T &defaultValue = {})
      {
        auto result = lambda(state);

        if (!result.has_value())
        {
          return defaultValue;
        }

        auto value = result.value();

        while (true)
        {
          result = lambda(state);
          if (!result.has_value())
          {
            break;
          }
          value = reduceLambda(value, result.value());
        }

        return value;
      }

      /**
       * @brief Binds a function to run each time an item comes out of the generator.
       *
       * This function wraps the existing generator function in another function, effectively binding extra logic to this generator.
       * each item on-the-fly as it's generated.
       *
       * @note This function does not consume the generator directly, it just runs the given function each time an item is generated.
       *
       * @param newLambda A function that takes a constant reference to an element of type `T` and returns nothing.
       * @return A reference to this generator, with the new function bound.
       */
      generator &forEach(std::function<void(const T &)> newLambda) noexcept
      {
        auto lambda = this->lambda;
        this->lambda = [lambda, newLambda](S &state)
        {
          auto item = lambda(state);
          if (item.has_value())
          {
            newLambda(item.value());
          }
          return item;
        };

        return *this;
      }

      /**
       * @brief Skips a certain number of items from the generator.
       *
       * This function will skip a certain number of items from the generator, or all items if there are fewer than the requested count.
       *
       * @note This function does not consume the generator directly, it just skips items as they are generated.
       *
       * @param count The number of items to skip.
       * @return A new generator that skips the given number of items.
       */
      generator<T, countedState> skip(long count) noexcept
      {
        auto lambda = this->lambda;

        return generator<T, countedState>({count, state}, [lambda](countedState &state)
                                          {
			for (long i = 0; i < state.count; i++) {
				auto item = lambda(state.state);
				if (!item.has_value()) {
					return item;
				}
			}
			state.count = 0;

			return lambda(state.state); });
      }

      /**
       * @brief Limits the number of items that the generator will std::optional.
       *
       * @param count The maximum number of items to std::optional.
       * @return A new generator that yields the given number of items.
       */
      generator<T, countedState> limit(long count) noexcept
      {
        auto lambda = this->lambda;

        return generator<T, countedState>({count, state}, [lambda](countedState &state)
                                          {
			if (state.count <= 0) {
				return std::optional<T>();
			}

			auto item = lambda(state.state);
			if (!item.has_value()) {
				return item;
			}

			state.count--;

			return item; });
      }

      /**
       * @brief Pair items from this generator with those of another generator.
       *
       * This function combines two generators into a single generator that yields pairs of items from both generators.
       * If one generator runs out of items, the resulting generator will stop yielding items.
       *
       * @param other The other generator to pair with.
       * @return A new generator that yields pairs of items from both generators.
       */
      template <typename U, typename S2>
      generator<std::pair<T, U>, generator<U, S2>> pair(generator<U, S2> &other) noexcept
      {
        typedef std::pair<T, U> pair_type;

        return generator<pair_type, generator<U, S2>>(other, [this](generator<U, S2> &otherGen)
                                                      {
			auto item1 = next();
			if (!item1.has_value()) {
				return std::optional<pair_type>();
			}

			auto item2 = otherGen.next();
			if (!item2.has_value()) {
				return std::optional<pair_type>();
			}

			return std::optional<pair_type>({item1.value(), item2.value()}); });
      }

      /**
       * @brief Zip this generator with another generator.
       *
       * This function combines two generators into a single generator that yields items from each
       * generator in an alternating pattern.
       * If one generator runs out of items, then only items from the other generator will be yielded.
       *
       * @param other The other generator to zip with.
       * @return A new generator that yields alternating items from both generators.
       */
      generator<T, std::pair<bool, generator &>> zip(generator &other) noexcept
      {
        return generator<T, std::pair<bool, generator &>>({false, other}, [this](std::pair<bool, generator &> &state)
                                                          {
			state.first = !state.first;

			// Draw from first generator
			if (state.first) {
				auto item = next();
				if (item.has_value()) {
					return item;
				}
				return state.second.next();
			}

			// Draw from second generator
			auto item = state.second.next();
			if (item.has_value()) {
				return item;
			}
			return next(); });
      }

      /**
       * @brief Enumerate the items in this generator.
       *
       * This function wraps the existing generator in another generator that yields pairs of indices and items.
       * The first item will have index 0, the second item will have index 1, and so on.
       *
       * @return A new generator that yields pairs of indices and items.
       */
      generator<std::pair<long, T>, std::pair<long, generator<T, S>>> enumerate() noexcept
      {
        return generator<std::pair<long, T>, std::pair<long, generator<T, S>>>({0, *this}, [](std::pair<long, generator<T, S>> &state) -> std::optional<std::pair<long, T>>
                                                                               {
			auto item = state.second.next();
			if (!item.has_value()) {
				return {};
			}
			return std::pair<long, T>{state.first++, item.value()}; });
      }

      /**
       * @brief List the items in this generator which differ from another generator.
       *
       * This function is only useful for generators that yield items of the same type.
       * (if the generators yielded different types, ALL items would be considered different!)
       *
       * As an example of how this works, if you have two generators that yield strings,
       * generator1 yields "apple", "banana", "cherry", "melon",
       * and generator2 yields "banana", "cherry", "date", "fig",
       * then calling generator1.diff(generator2) will yield "apple" and "melon".
       *
       * @note If the other generator runs out of items, the rest of the items from this generator will be yielded,
       * as they are considered different from nothing.
       *
       * @param other The other generator to compare against.
       * @return A new generator that yields only items that are different from the items in the other generator.
       */
      generator<T, std::pair<generator, std::optional<T>>> diff(generator &other) noexcept
      {
        return generator<T, std::pair<generator, std::optional<T>>>({other, other.next()}, [this](std::pair<generator, std::optional<T>> &state) -> std::optional<T>
                                                                    {
			while (true) {
				auto item1 = next();
				if (!item1.has_value()) {
					return {};
				}

				if (!state.second.has_value()) {
					return item1.value(); // Yield the item from this generator, as the other generator is done
				}

				if (item1.value() != state.second.value()) {
					return item1.value(); // Yield the item from this generator, as it is different
				}

				// Move to the next item in the other generator
				state.second = state.first.next();
			} });
      }

      /**
       * @brief Get chunks of items from the generator.
       *
       * This function will yield chunks of items from the generator,
       * where each chunk is an array of items of *at most* the specified size.
       * If the generator runs out of items, the last chunk may contain fewer items.
       *
       * @param chunkSize The size of each chunk.
       * @return A new generator that yields arrays of items, each of *at most* the specified size.
       */
      generator<std::vector<T>, generator> chunk(long chunkSize) noexcept
      {
        return generator<std::vector<T>, generator>(*this, [chunkSize](generator &state) -> std::optional<std::vector<T>>
                                                    {
                                                      std::vector<T> chunk;
                                                      for (long i = 0; i < chunkSize; i++)
                                                      {
                                                        auto item = state.next();
                                                        if (!item.has_value())
                                                        {
                                                          if (chunk.length() == 0)
                                                          {
                                                            return {}; // No more items, end the generator
                                                          }
                                                          break; // The chunk has data, yield it
                                                        }
                                                        chunk.push(item.value());
                                                      }
                                                      return chunk; // Return the current chunk
                                                    });
      }

      /**
       * @brief Allow peeking at the next item in the generator as items are generated.
       *
       * The generator that this function produces will yield std::pairs containing (1) the
       * current value, and (2) a yield object containing the next value, if any.
       * It is up to the programmer to properly check that the yield has a value.
       *
       * @return A new generator that yields a pair of (item, z::core::std::optional<item>).
       */
      generator<std::pair<T, std::optional<T>>, std::pair<std::optional<T>, generator>> peek() noexcept
      {
        return generator<std::pair<T, std::optional<T>>, std::pair<std::optional<T>, generator>>({next(), *this}, [](std::pair<std::optional<T>, generator> &state) -> std::optional<std::pair<T, std::optional<T>>>
                                                                                                 {
			const auto prevValue = state.first;
			auto &gen = state.second;

			if (!prevValue.has_value()) {
				// No more items, end the generator
				return {};
			}
			auto nextValue = gen.next();
			state.first = nextValue;

			return std::pair<T, std::optional<T>>(prevValue.value(), nextValue); });
      }

      /**
       * @brief Chains two generators together.
       *
       * This function takes two generators and chains them together,
       * so that multiple generators can be used as a single generator.
       * That is, the items from this are generated first, and once this
       * generator is exhausted, items from the other generator are
       * generated until exhaustion.
       *
       * @note Generators chained together this way \b must yield values of the same type.
       *
       * @tparam U The state type of the other generator.
       * @param other The generator to chain after this generator.
       * @return A new generator that first yields items from this generator and then the other.
       */
      template <typename U>
      generator<T, std::pair<generator, bool>> chain(generator<T, U> &other) noexcept
      {
        return generator<T, std::pair<generator, bool>>({*this, false}, [&other](std::pair<generator, bool> &state)
                                                        {
			if (!state.second) {
				auto item = state.first.next();
				if (!item.has_value()) {
					state.second = true;
				} else {
					return item;
				}
			}

			return other.next(); });
      }

      /**
       * @copydoc chain()
       */
      template <typename U>
      generator<T, std::pair<std::pair<generator, bool>, generator<T, U>>> chain(generator<T, U> &&other) noexcept
      {
        return generator<T, std::pair<std::pair<generator, bool>, generator<T, U>>>({{*this, false}, other}, [&other](std::pair<std::pair<generator, bool>, generator<T, U>> &state)
                                                                                    {
			if (!state.first.second) {
				auto item = state.first.first.next();
				if (!item.has_value()) {
					state.first.second = true;
				} else {
					return item;
				}
			}

			return state.second.next(); });
      }

      /**
       * @brief End the generator when the given predicate returns true.
       *
       * @note This does \b not yield the item for which the predicate was true.
       *
       * @param predicate A function or lambda that returns true if the generator should
       * stop yielding items, or false if it should continue.
       * @return A new generator that stops when the predicate returns true.
       */
      generator until(std::function<bool(T)> predicate) noexcept
      {
        auto lambda = this->lambda;

        return generator(state, [lambda, predicate](S &state)
                         {
			auto val = lambda(state);
			if (val.has_value() && predicate(val.value())) {
				return std::optional<T>{};
			}
			return val; });
      }

      /**
       * @brief End the generator when the given value is yielded from the generator.
       *
       * @note This does \b not yield the item that matches the sentinel.
       *
       * @param sentinel The value to check for.
       * @return A new generator that stops when it finds the given value.
       */
      generator until(const T &sentinel) noexcept
      {
        auto lambda = this->lambda;

        return generator(state, [lambda, sentinel](S &state)
                         {
			auto val = lambda(state);
			if (val.has_value() && val.value() == sentinel) {
				return std::optional<T>{};
			}
			return val; });
      }

      /**
       * @see chain()
       *
       * @copydoc chain()
       */
      template <typename U>
      inline generator<T, std::pair<generator, bool>> operator+(generator<T, U> &other) noexcept
      {
        return chain(other);
      }

      /**
       * @see chain()
       *
       * @copydoc chain()
       */
      template <typename U>
      inline generator<T, std::pair<std::pair<generator, bool>, generator<T, U>>> operator+(generator<T, U> &&other) noexcept
      {
        return generator<T, std::pair<std::pair<generator, bool>, generator<T, U>>>({{*this, false}, other}, [&other](std::pair<std::pair<generator, bool>, generator<T, U>> &state)
                                                                                    {
			if (!state.first.second) {
				auto item = state.first.first.next();
				if (!item.has_value()) {
					state.first.second = true;
				} else {
					return item;
				}
			}

			return state.second.next(); });
      }

      /**
       * @see map()
       *
       * @copydoc map()
       */
      template <typename U>
      inline generator<U, S> operator|(std::function<U(T)> mapLambda) noexcept
      {
        return map<U>(mapLambda);
      }

      /**
       * @see map()
       *
       * @copydoc map()
       */
      template <typename U>
      inline generator<U, S> operator|(U (*mapLambda)(T)) noexcept
      {
        return map<U>(mapLambda);
      }

      /**
       * @see filter()
       *
       * @copydoc filter()
       */
      inline generator operator&&(std::function<T(const T &)> filterLambda) noexcept
      {
        return filter(filterLambda);
      }

      /**
       * @see reduce()
       *
       * @brief Reduces the generator to a single value by applying a binary operation cumulatively to all yielded values.
       *
       * This function applies a binary operation (provided as a lambda) to combine all yielded items into a single value.
       * If the generator doesn't yield anything, a default value (created via the default `{}` constructor) is returned.
       *
       * @note Especially for long lists of items, this is significantly more memory-efficient than calling collect() and then
       * reduce() on the resulting array, as an intermediate array does not need to be constructed.
       *
       * @param reduceLambda A function that takes two elements of type `T` and returns their combined result of type `T`.
       * @return The result of the reduction operation.
       */
      inline T operator>>(std::function<T(const T &, const T &)> reduceLambda)
      {
        return reduce(reduceLambda);
      }

      /**
       * @see until(std::function<bool(T)>)
       *
       * @copydoc until(std::function<bool(T)>)
       */
      inline generator operator!=(std::function<bool(T)> predicate)
      {
        return until(predicate);
      }

      /**
       * @see until(const T&)
       *
       * @copydoc until(const T&)
       */
      inline generator operator!=(const T &sentinel)
      {
        return until(sentinel);
      }
    };

    /**
     * @brief Create a generator from an initializer list.
     * @tparam T The type of value that the iterable returns.
     * @param list The initializer list to create a generator from.
     * @return A generator that will yield the items from the initializer list.
     */
    template <typename T>
    generator<T, std::pair<std::vector<T>, long>> generatorFrom(std::initializer_list<T> list)
    {
      return generator<T, std::pair<std::vector<T>, long>>({list, 0}, [](std::pair<std::vector<T>, long> &state) -> std::optional<T>
                                                           {
		if (state.second < state.first.length()) {
			return state.first[state.second++];
		}

		return {}; });
    }

    /**
     * @brief Create a generator from a map.
     * @tparam K The type of the key in the map.
     * @tparam V The type of the value in the map.
     * @param map The map to create a generator from.
     * @return A generator that will yield pairs of key-value from the map.
     */
    template <typename K, typename V>
    generator<std::pair<K, V>, typename std::map<K, V>::const_iterator> generatorFrom(const std::map<K, V> &map)
    {
      return generator<std::pair<K, V>, typename std::map<K, V>::const_iterator>(map.begin(), [&map](auto &iter) -> std::optional<std::pair<K, V>>
                                                                                 {
		if (iter != map.end()) {
			auto ret = *iter;
			++iter; // Move to the next item
			return ret;
		}

		return {}; });
    }

    /**
     * @brief Create a generator from a temporary map.
     * @tparam K The type of the key in the map.
     * @tparam V The type of the value in the map.
     * @param map The map to create a generator from.
     * @return A generator that will yield pairs of key-value from the map.
     */
    template <typename K, typename V>
    generator<std::pair<K, V>, std::pair<typename std::map<K, V>::const_iterator, std::map<K, V>>> generatorFrom(std::map<K, V> &&map)
    {
      return generator<std::pair<K, V>, std::pair<typename std::map<K, V>::const_iterator, std::map<K, V>>>({map.begin(), map}, [](auto &state) -> std::optional<std::pair<K, V>>
                                                                                                            {
		if (state.first != state.second.end()) {
			auto ret = *state.first;
			++state.first; // Move to the next item
			return ret;
		}

		return {}; });
    }

  } // namespace core
} // namespace z