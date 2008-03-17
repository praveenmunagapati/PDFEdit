/*
 * PDFedit - free program for PDF document manipulation.
 * Copyright (C) 2006, 2007, 2008  PDFedit team: Michal Hocko,
 *                                              Miroslav Jahoda,
 *                                              Jozef Misutka,
 *                                              Martin Petricek
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program (in doc/LICENSE.GPL); if not, write to the 
 * Free Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, 
 * MA  02111-1307  USA
 *
 * Project is hosted on http://sourceforge.net/projects/pdfedit
 */
// vim:tabstop=4:shiftwidth=4:noexpandtab:textwidth=80

#ifndef _CPAGECONTENTS_H_
#define _CPAGECONTENTS_H_

// static includes
#include "kernel/static.h"
#include "kernel/cpagemodule.h"

#include "kernel/iproperty.h"
#include "kernel/ccontentstream.h"
#include "kernel/textoutput.h"
#include "kernel/textsearchparams.h"
#include "kernel/stateupdater.h"


//==========================================================
namespace pdfobjects {
//==========================================================

// Forward declaration
class CPage;
class CDict;


//==========================================================
// CPageContents
//==========================================================

/**
 * Class representing the Contents entry in a page.
 * Provides convinient access and modify operations on "Contents" entry of a page dictionary.
 */
class CPageContents : public ICPageModule
{

	//==========================================================
	// Contents observer
	//==========================================================
private:

	/** 
	 * Observer implementation for content stream synchronization.
	 */
	class ContentsWatchDog: public IPropertyObserver
	{
	private:
		CPageContents* _cnt;
	public:
		ContentsWatchDog (CPageContents* cnt) : _cnt(cnt) { assert(_cnt); }
		virtual ~ContentsWatchDog() throw() {}
		// IPropertyObserver Interface
		virtual void notify (boost::shared_ptr<IProperty>, boost::shared_ptr<const IProperty::ObserverContext>) const throw();
		virtual priority_t getPriority() const throw() 
			{ return 0;	}
	
	};	// class ContentsWatchDog

	//==========================================================

	// Typedefs
private:
	typedef std::vector<boost::shared_ptr<CContentStream> > CCs;

	// Variables
private:
	CCs _ccs;		// content streams
	CPage* _page;	// pages
	boost::shared_ptr<CDict> _dict;	// pages
	boost::shared_ptr<ContentsWatchDog> _wd; // content streams


	// Ctor & Dtor
public:
	CPageContents (CPage* page);
	~CPageContents ();

	//
	// ICPageModule interface
	//
public:
	/** @see ICPageModule::reset */
	virtual void reset ();


	//
	// Methods
	//
public:

	/**
	 * Reparse content stream using actual display parameters. 
	 */
	void reparse ();

	/**
	 * Add new content stream to the front. This function adds new entry in the "Contents"
	 * property of a page. The container of provided operators must form a valid
	 * contentstream.
	 * This function should be used when supplied operators
	 * should be handled at the beginning end e.g. should be drawn first which means
	 * they will appear the "below" other object.
	 *
	 * This function can be used to separate our changes from original content stream.
	 * Indicats that the page changed.
	 *
	 * @param cont Container of operators to add.
	 */
	template<typename Container>
	void addToFront (const Container& cont);

	/**
	 * Add new content stream to the back. This function adds new entry in the "Contents"
	 * property of a page. The container of provided operators must form a valid
	 * contentstream. 
	 * This function should be used when supplied operators
	 * should be handled at the end e.g. should be drawn at the end which means
	 * they will appear "above" other objects.
	 *
	 * This function can be used to separate our changes from original content stream.
	 * Indicats that the page changed.
	 *
	 * @param cont Container of operators to add.
	 */
	template<typename Container> 
	void addToBack (const Container& cont);

	/**
	 * Remove content stream. 
	 * This function removes all objects from "Contents" entry which form specified contentstream.
	 * Indicats that the page changed.
	 *
	 * @param csnum Number of content stream to remove.
	 */
	void remove (size_t csnum);

	/**
	 * Find all occurences of a text on this page.
	 *
	 * It uses xpdf TextOutputDevice to get the bounding box of found text.
	 *
	 * @param text Text to find.
	 * @param recs Output container of rectangles of all occurences of the text.
	 * @param params Search parameters.
	 *
	 * @return Number of occurences found.
	 */
	 template<typename RectangleContainer>
	 size_t findText (std::string text, 
					  RectangleContainer& recs, 
					  const TextSearchParams& params = TextSearchParams()) const;


	/**
	 * Get text source of a page.
	 */
	template<typename WordEngine, 
			 typename LineEngine, 
			 typename ColumnEngine>
 	void convert (textoutput::OutputBuilder& out)
	{
			kernelPrintDbg (debug::DBG_INFO, ""); 

		typedef textoutput::PageTextSource<WordEngine, LineEngine, ColumnEngine> TextSource;

		// Create gfx resource and state
		boost::shared_ptr<GfxResources> gfxres;
		boost::shared_ptr<GfxState> gfxstate;
		_xpdf_display_params (gfxres, gfxstate);
			assert (gfxres && gfxstate);

		// Create page text class with parametrized parts
		TextSource text_source;

		// Get text from all content streams
		for (CCs::iterator it = _ccs.begin(); it != _ccs.end(); ++it)
		{
			// Get operators and build text representation if not empty
			CContentStream::Operators ops;
			(*it)->getPdfOperators (ops);
			if (!ops.empty())
			{
				PdfOperator::Iterator itt = PdfOperator::getIterator (ops.front());
				StateUpdater::updatePdfOperators<TextSource&> (itt, gfxres, *gfxstate, text_source);
			}
		}

		// Create lines, columns...
		text_source.format ();
		// Build the output
		if (hasValidPdf(_dict))
			text_source.output (out, _page_pos());
		else
			text_source.output (out, 0);
	}

	//
	// Getters & Setters
	//
public:

	/**
	 * Get pdf operators at specified position. 
	 * This call will be delegated to content stream object.
	 *
	 * @param opContainer Operator container where operators in specified are wil be stored.
	 * @param cmp 	Null if default kernel area comparator should be used otherwise points 
	 * 				 to an object which will decide whether an operator is "near" a point.
	 */
	template<typename OpContainer, typename PositionComparator>
	void getObjectsAtPosition (OpContainer& opContainer, PositionComparator cmp)
	{	
		init();
		// Get the objects with specific comparator
		for (CCs::iterator it = _ccs.begin (); it != _ccs.end(); ++it)
			(*it)->getOperatorsAtPosition (opContainer, cmp);
	}

	/** 
	 * Returns shared pointer to the specified content stream. 
	 */
	boost::shared_ptr<CContentStream> getContentStream (CContentStream* cc); 

	/**
	 * Returns shared pointer to the specified content stream. 
	 */
	boost::shared_ptr<CContentStream> getContentStream (size_t pos); 

	/** 
	 * Fills container with contents streams. 
	 */
	template<typename Container> 
	void getContentStreams (Container& container)
	{
		init();
		container.clear();
		std::copy (_ccs.begin(), _ccs.end(), std::back_inserter(container));
	}

	/**  
	 * Returns plain text extracted from a page using xpdf code.
	 * 
	 * This method uses xpdf TextOutputDevice that outputs a page to a text device.
	 * Text in a pdf is stored neither word by word nor letter by letter. It is not
	 * easy not decide whether two letters form a word. Xpdf uses insane
	 * algorithm that works most of the time.
	 *
	 * @param text Output string  where the text will be saved.
	 * @param encoding Encoding format.
	 * @param rc Rectangle from which to extract the text.
	 */
	void getText (std::string& text, 
				  const std::string* encoding = NULL, 
				  const libs::Rectangle* rc = NULL) const;
 
	/**
	 * Move contentstream up one level. Which means it will be repainted by less objects.
	 */
	void moveAbove (boost::shared_ptr<const CContentStream> ct);
	void moveAbove (size_t pos);

	/**
	 * Move contentstream below one level. Which means it will be repainted by more objects.
	 */
	void moveBelow (boost::shared_ptr<const CContentStream> ct);
	void moveBelow (size_t pos);



	//
	// CCs methods
	//
public:

	/** 
	 * Set Contents entry from a container of content streams. 
	 * Indicats that the page changed.
	 */
	template<typename Cont> 
	static void setContents (boost::shared_ptr<CDict> dict, const Cont& cont);

	//
	// CCs helper methods
	//
private:
	/** Add ref to front. */
	void toFront (CRef& ref);
	/** Add ref to back. */
	void toBack (CRef& ref);

	/** Remove content streams references from Contents entry. */
	void remove (boost::shared_ptr<const CContentStream> cs);
	/** Remove one indiref from Contents entry. */
	void remove (const IndiRef& rf);

	/**
	 * Parse content stream. 
	 * Content stream is an optional property. When found it is parsed,
	 * nothing is done otherwise.
	 *
	 * @return True if content stream was found and was parsed, false otherwise.
	 */
	bool parse ();

	//
	// Helper methods
	//
private:

	/** 
	 * Init ccs only when necessary. 
	 */
	inline void init ()
	{
		if (_ccs.empty())
			parse ();		
	}

	/** 
	 * Indicate changed page. 
	 */
	inline void change (bool invalid = false);

	//
	// Helper methods because of cpage not included in headers
	//
private:
	/**
	 * Get xpdf display params.
	 */
	void _xpdf_display_params (boost::shared_ptr<GfxResources>& res, 
							   boost::shared_ptr<GfxState>& state);
	/**
	 * Get xpdf display params.
	 */
	size_t _page_pos () const;


	//
	// Helper methods because of cpage not included in headers
	//
private:

	/**
	 * Register content stream observer either on page dictionary or supplied object
	 * if valid.
	 * The observer is registered on page dictionary and on its Contents entry (if any).
	 *
	 * @param ip property to register content stream observer
	 */
	void reg_observer (boost::shared_ptr<IProperty> ip = boost::shared_ptr<IProperty>()) const;

	/**
	 * Unregister observer from page dictionary or supplied object if valid.
	 *
	 * @param ip property to unregister content stream observer
	 */
	void unreg_observer (boost::shared_ptr<IProperty> ip = boost::shared_ptr<IProperty>()) const;


	/**
	 * Contents inverse lock for observers. When initialized the changes are not dispatched.
	 */
	struct ContentsObserverFreeSection
	{
		CPageContents* _cnt;
		ContentsObserverFreeSection (CPageContents* cnt) : _cnt (cnt)
			{ _cnt->unreg_observer(); }
		~ContentsObserverFreeSection () 
			{ _cnt->reg_observer();}
	};

}; // class CPageContents

//==========================================================
} // namespace pdfobjects
//==========================================================


#endif // _CPAGECONTENTS_H_

