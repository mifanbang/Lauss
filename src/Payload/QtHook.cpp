/*
 *  Lauss - PoC blocking ad banners in LINE clients on Windows
 *  Copyright (C) 2023 Mifan Bang <https://debug.tw>.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "Debug.h"
#include "QtHook.h"

#include <shlwapi.h>
#pragma comment(lib, "shlwapi.lib")

#include <cstdio>
#include <string>


decltype(&QWidget::show) Hook_QWidget::s_trampoline{ nullptr };
size_t Hook_QWidget::s_outputIndentCount{ };


// If for any reason s_trampoline is null while this hook function gets called,
// we should just let LINE crash.
void Hook_QWidget::Show()
{
	const std::string outputIndent(s_outputIndentCount, ' ');

	QString str = (this->*ObjectName)();
	if (str.data)
	{
		Printf(
			"%s[QWidget::Show()] parent=%p this=%p name=%S\n",
			outputIndent.c_str(),
			(this->*ParentWidget)(),
			this,
			str.data->Data()
		);

		if (StrStrIW(str.data->Data(), L"bannerWholeImage"))
		{
			if (QWidget* parent1st = (this->*ParentWidget)())
			{
				if (QWidget* parent2nd = (parent1st->*ParentWidget)())
				{
					(parent2nd->*Resize)(0, 0);
					return;
				}
			}

			// If we reached here, LINE must have changed its UI design or behavior.
			Printf("%s[QWidget::Show()] Failed to hide ad banner. It seems the LINE version is too new and not yet supported.\n", outputIndent.c_str());
		}
	}

	++s_outputIndentCount;
	(this->*s_trampoline)();
	--s_outputIndentCount;

	Printf("%s[QWidget::Show()] Returned from original.\n", outputIndent.c_str());
}
