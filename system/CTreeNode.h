#pragma once
#include <vector>
#include <memory>

template <typename T>
class CTreeNode 
{
public:
	T m_nodedata;
	CTreeNode<T>* m_parent{};
	std::vector<std::unique_ptr<CTreeNode<T>>> m_children;

	void Addchild(std::unique_ptr<CTreeNode<T>> child) {
		
		m_children.push_back(std::move(child));
	}	
};