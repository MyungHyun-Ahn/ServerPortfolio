#pragma once

// ����Ʈ �����͸� ����� Ŭ������
// ���������� ���Ͷ��� ����Ͽ� ���۷��� ī��Ʈ�� �����ϴ� �������̽� ����
//   * LONG IncreaseRef();
//   * LONG DecreaseRef();
// T::Free() �������̽��� ����
// ��Ʈ��ũ ���̺귯�������� �� IncreaseRef(), DecreaseRef() �Լ��� ���� ���� ���۷��� ī��Ʈ�� ��� ����
//   * ������ �ڵ忡���� �Ʒ��� CSmartPtr ��� 

#include <windows.h>
#include <concepts>

namespace MHLib::utils
{
	template<typename T>
	concept SmartPtrCompatible = requires(T t)
	{
		{ t.IncreaseRef() } -> std::same_as<LONG>;
		{ t.DecreaseRef() } -> std::same_as<LONG>;
		{ T::Free(std::declval<T *>()) } -> std::same_as<void>;
	};

	template<SmartPtrCompatible T>
	class CSmartPtr
	{
	public:
		CSmartPtr() noexcept = default;

		// ���� �� ����
		CSmartPtr(T *ptr) noexcept
		{
			m_ptr = ptr;
			m_ptr->IncreaseRef();
		}

		// ����� ȣ���ص� ��
		~CSmartPtr() noexcept
		{
			// 0�� �Ǹ�
			if (m_ptr != nullptr && m_ptr->DecreaseRef() == 0)
				T::Free(m_ptr);

			m_ptr = nullptr;
		}

		CSmartPtr(const CSmartPtr &sPtr) noexcept
		{
			m_ptr = sPtr.m_ptr;
			m_ptr->IncreaseRef();
		}


		CSmartPtr &operator=(const CSmartPtr &sPtr) noexcept
		{
			m_ptr = sPtr.m_ptr;
			m_ptr->IncreaseRef();
			return *this;
		}

		T &operator*() noexcept
		{
			return *m_ptr;
		}

		T *operator->() noexcept
		{
			return m_ptr;
		}

		inline static CSmartPtr MakeShared()  noexcept
		{
			T *ptr = T::Alloc();
			return CSmartPtr(ptr);
		}

	public:
		inline T *GetRealPtr() const noexcept { return m_ptr; }

	private:
		T *m_ptr = nullptr;
	};
}