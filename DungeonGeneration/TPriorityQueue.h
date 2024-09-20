// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

/**
 * 
 */
template<class T>
class NETWORKINGPROTOTYPE_API TPriorityQueue
{
public:
	TPriorityQueue()
	{
		Heap.Heapify();
	}
	
	// Always check if IsEmpty() before Pop-ing!
	T Pop();
	void Push(T item);
	void Empty();
	bool IsEmpty() const;
	int32 Num() const;

private:
	TArray<T> Heap;
};

template <class T>
T TPriorityQueue<T>::Pop()
{
	if (IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("Attempting to dequeue from an empty queue!"));
		return T(); // Return default-constructed element
	}

	T Root;
	Heap.HeapPop(Root);
	return Root;
}

template <class T>
void TPriorityQueue<T>::Push(T item)
{
	Heap.HeapPush(item);
}

template <class T>
void TPriorityQueue<T>::Empty()
{
	Heap.Empty();
}

template <class T>
bool TPriorityQueue<T>::IsEmpty() const
{
	return Heap.Num() == 0;
}

template <class T>
int32 TPriorityQueue<T>::Num() const
{
	return Heap.Num();
}
