// vim:tabstop=4:shiftwidth=4:noexpandtab:textwidth=80

/*
 * $RCSfile$
 *
 * $Log$
 * Revision 1.17  2006/05/08 21:24:33  hockm0bm
 * checkLinearized function added
 *
 * Revision 1.16  2006/05/08 20:17:42  hockm0bm
 * * key words removed from here
 * * new model of data writing
 *         - uses IPdfWriter interface for implementation specific writer
 *         - uses OldStylePdfWriter by default
 *         - new implementator can be set by setPdfWriter method
 *         - saveChanges collects object from changedStorage and uses IPdfWriter
 *           to write them to the stream so it is independed on implementation
 *
 * Revision 1.15  2006/05/06 08:40:20  hockm0bm
 * knowsRef delegates to XRef::knowsRef if not in the newest revision
 *
 * Revision 1.14  2006/05/01 13:53:08  hockm0bm
 * new style printDbg
 *
 * Revision 1.13  2006/04/27 18:21:09  hockm0bm
 * * deallocation of Object corrected
 * * changeRevision
 *         - saves revision
 *
 * Revision 1.12  2006/04/23 13:13:20  hockm0bm
 * * getRevisionEnd method added
 *         - to get end of stream contnet for current revision
 * * cloneRevision method added
 *         - to support cloning of pdf in certain revision
 *
 * Revision 1.11  2006/04/21 20:38:08  hockm0bm
 * saveChanges writes indirect objects correctly
 *         - uses createIndirectObjectStringFromString method
 *
 * Revision 1.10  2006/04/20 22:32:09  hockm0bm
 * just debug message added
 *
 * Revision 1.9  2006/04/19 06:00:23  hockm0bm
 * * changeRevision - first implementation (not tested yet)
 * * collectRevisions -  first implementation (not tested yet)
 * * minor changes in saveChanges
 *
 * Revision 1.8  2006/04/17 19:57:30  hockm0bm
 * * findPDFEof removed
 *         - uses XRef eofPos value instead
 * * string constants for pdf markers
 *         - TRAILER_KEYWORD, XREF_KEYWORD, STARTXREF_KEYWORD
 * * minor changes in saveChanges method and buildXref
 *         - first testing - seems to work, but needs to test
 *
 * Revision 1.7  2006/04/13 18:08:49  hockm0bm
 * * releaseObject method removed
 * * readOnly mode removed - makes no sense in here
 * * documentation updated
 * * contains information about CPdf
 *         - because of pdf read only mode
 * * buildXref method added
 * * XREFROWLENGHT and EOFMARKER macros added
 *
 * Revision 1.6  2006/04/12 17:52:25  hockm0bm
 * * saveChanges method replaces saveXRef method
 *         - new semantic for saving changes
 *              - temporary save
 *              - new revision save
 *         - storePos field added to mark starting
 *           place where to store changes
 *         - saveChanges moves with storePos if new
 *           revision is should be created
 * * findPDFEof function added
 * * StreamWriter in place of BaseStream
 *         - all changes will be done via StreamWriter
 *
 * Revision 1.5  2006/03/23 22:13:51  hockm0bm
 * printDbg added
 * exception handling
 * TODO removed
 * FIXME for revisions handling - throwing exeption
 *
 *
 */
#include "xrefwriter.h"
#include "cobject.h"

using namespace pdfobjects;
using namespace utils;
using namespace debug;

#ifndef FIRST_LINEARIZED_BLOCK
/** Size of first block of linearized pdf where Linearized dictionary must
 * occure. 
 * 
 * If it value is not defined yet, uses 1024 default value. So it may be
 * specified as compile time define for this module.
 */
#define FIRST_LINEARIZED_BLOCK 1024
#endif

namespace utils {

bool checkLinearized(StreamWriter & stream, Ref * ref)
{
	// searches num gen obj entry. Starts from stream begining
	stream.reset();
	Object obj;
	Parser parser=Parser(
			NULL, new Lexer(
				NULL, stream.makeSubStream(stream.getPos(), false, 0, &obj)
				)
			);

	while(stream.getPos() < FIRST_LINEARIZED_BLOCK)
	{
		Object obj1, obj2, obj3;
		parser.getObj(&obj1);
		parser.getObj(&obj2);
		parser.getObj(&obj3);

		// indirect object must start with 
		// num gen obj line
		if(obj1.isInt() && obj2.isInt() && obj3.isCmd())
		{
			Object obj;
			parser.getObj(&obj);

			//  result is false by default - it MUST BE linearized dictionary
			bool result=false;
			
			if(obj.isDict())
			{
				// indirect object is dictionary, so it can be Linearized
				// dictionary
				Object version;
				obj.getDict()->lookupNF("/Linearized", &version);
				if(version.isInt())
				{
					// this is realy Linearized dictionary, so stream contains
					// lienarized pdf content.
					// if ref is not NULL, sets reference of this dictionary
					if(ref)
					{
						ref->num=obj1.getInt();
						ref->gen=obj2.getInt();
					}
					result=true;
				}
			}

			obj1.free();
			obj2.free();
			obj3.free();
			return result;
		}

		obj1.free();
		obj2.free();
		obj3.free();
	}

	// no indirect object in the document
	return false;
}

} // end of utils namespace

XRefWriter::XRefWriter(StreamWriter * stream, CPdf * _pdf)
	:CXref(stream), 
	mode(paranoid), 
	pdf(_pdf), 
	revision(0), 
	pdfWriter(new OldStylePdfWriter())
{
	// gets storePos
	// searches %%EOF element from startxref position.
	storePos=XRef::eofPos;

	// collects all available revisions
	collectRevisions();
}

IPdfWriter * XRefWriter::setPdfWriter(IPdfWriter * writer)
{
	IPdfWriter * current=pdfWriter;

	// if given writer is non NULL, sets pdfWriter
	if(writer)
		pdfWriter=writer;

	return current;
}

bool XRefWriter::paranoidCheck(::Ref ref, ::Object * obj)
{
	bool reinicialization=false;

	kernelPrintDbg(DBG_DBG, "ref=["<<ref.num<<", "<<ref.gen<<"] type="<<obj->getType());

	if(mode==paranoid)
	{
		// reference known test
		if(!knowsRef(ref))
		{
			// if reference is not known, it may be new 
			// which is not changed after initialization
			if(!(reinicialization=newStorage.contains(ref)))
			{
				kernelPrintDbg(DBG_WARN, "ref=["<<ref.num<<", "<<ref.gen<<"] is unknown");
				return false;
			}
		}
		
		// type safety test only if object has initialized 
		// value already (so new and not changed are not test
		if(!reinicialization)
		{
			// gets original value and uses typeSafe to 
			// compare with given value type
			::Object original;
			fetch(ref.num, ref.gen, &original);
			ObjType originalType=original.getType();
			bool safe=typeSafe(&original, obj);
			original.free();
			if(!safe)
			{
				kernelPrintDbg(DBG_WARN, "ref=["<<ref.num<<", "<<ref.gen<<"] type="<<obj->getType()
						<<" is not compatible with original type="<<originalType);
				return false;
			}
		}
	}

	kernelPrintDbg(DBG_INFO, "paranoidCheck successfull");
	return true;
}

void XRefWriter::changeObject(int num, int gen, ::Object * obj)
{
	kernelPrintDbg(DBG_DBG, "ref=["<< num<<", "<<gen<<"]");

	if(revision)
	{
		// we are in later revision, so no changes can be
		// done
		kernelPrintDbg(DBG_ERR, "no changes available. revision="<<revision);
		throw ReadOnlyDocumentException("Document is not in latest revision.");
	}
	if(pdf->getMode()==CPdf::ReadOnly)
	{
		// document is in read-only mode
		kernelPrintDbg(DBG_ERR, "pdf is in read-only mode.");
		throw ReadOnlyDocumentException("Document is in Read-only mode.");
	}

	::Ref ref={num, gen};
	
	// paranoid checking
	if(!paranoidCheck(ref, obj))
	{
		kernelPrintDbg(DBG_ERR, "paranoid check for ref=["<< num<<", "<<gen<<"] not successful");
		throw ElementBadTypeException("" /* FIXME "[" << num << ", " <<gen <<"]" */);
	}

	// everything ok
	CXref::changeObject(ref, obj);
}

::Object * XRefWriter::changeTrailer(const char * name, ::Object * value)
{
	kernelPrintDbg(DBG_DBG, "name="<<name);
	if(revision)
	{
		// we are in later revision, so no changes can be
		// done
		kernelPrintDbg(DBG_ERR, "no changes available. revision="<<revision);
		throw ReadOnlyDocumentException("Document is not in latest revision.");
	}
	if(pdf->getMode()==CPdf::ReadOnly)
	{
		// document is in read-only mode
		kernelPrintDbg(DBG_ERR, "pdf is in read-only mode.");
		throw ReadOnlyDocumentException("Document is in Read-only mode.");
	}
		
	// paranoid checking - can't use paranoidCheck because value may be also
	// direct - we are in trailer
	if(mode==paranoid)
	{
		::Object original;
		
		kernelPrintDbg(DBG_DBG, "mode=paranoid type safety is checked");
		// gets original value of value
		Dict * dict=trailerDict.getDict();
		dict->lookupNF(name, &original);
		bool safe=typeSafe(&original, value);
		original.free();
		if(!safe)
		{
			kernelPrintDbg(DBG_ERR, "type safety error");
			throw ElementBadTypeException(name);
		}
	}

	// everything ok
	return CXref::changeTrailer(name, value);
}

::Ref XRefWriter::reserveRef()
{
	kernelPrintDbg(DBG_DBG, "");

	// checks read-only mode
	
	if(revision)
	{
		// we are in later revision, so no changes can be
		// done
		kernelPrintDbg(DBG_ERR, "no changes available. revision="<<revision);
		throw ReadOnlyDocumentException("Document is not in latest revision.");
	}
	if(pdf->getMode()==CPdf::ReadOnly)
	{
		// document is in read-only mode
		kernelPrintDbg(DBG_ERR, "pdf is in read-only mode.");
		throw ReadOnlyDocumentException("Document is in Read-only mode.");
	}

	// changes are availabe
	// delegates to CXref
	return CXref::reserveRef();
}


::Object * XRefWriter::createObject(::ObjType type, ::Ref * ref)
{
	kernelPrintDbg(DBG_DBG, "type="<<type);

	// checks read-only mode
	
	if(revision)
	{
		// we are in later revision, so no changes can be
		// done
		kernelPrintDbg(DBG_ERR, "no changes available. revision="<<revision);
		throw ReadOnlyDocumentException("Document is not in latest revision.");
	}
	if(pdf->getMode()==CPdf::ReadOnly)
	{
		// document is in read-only mode
		kernelPrintDbg(DBG_ERR, "pdf is in read-only mode.");
		throw ReadOnlyDocumentException("Document is in Read-only mode.");
	}

	// changes are availabe
	// delegates to CXref
	return CXref::createObject(type, ref);
}

void XRefWriter::saveChanges(bool newRevision)
{
	kernelPrintDbg(DBG_DBG, "");

	// if changedStorage is empty, there is nothing to do
	if(changedStorage.size()==0)
	{
		kernelPrintDbg(DBG_INFO, "Nothing to be saved - changedStorage is empty");
		return;
	}
	
	// casts stream (from XRef super type) and casts it to the FileStreamWriter
	// instance - it is ok, because it is initialized with this type of stream
	// in constructor
	StreamWriter * streamWriter=dynamic_cast<StreamWriter *>(XRef::str);

	// gets vector of all changed objects
	IPdfWriter::ObjectList changed;
	ObjectStorage< ::Ref, ObjectEntry *, RefComparator>::Iterator i;
	for(i=changedStorage.begin(); i!=changedStorage.end(); i++)
	{
		::Ref ref=i->first;
		Object * obj=i->second->object;
		changed.push_back(IPdfWriter::ObjectElement(ref, obj));
	}

	// delegates writing to pdfWriter using streamWriter stream from storePos
	// position.
	// Stores position of the cross reference section to xrefPos
	if(!pdfWriter)
	{
		kernelPrintDbg(DBG_ERR, "No pdfWriter defined");
		return;
	}
	pdfWriter->writeContent(changed, *streamWriter, storePos);
	size_t xrefPos=streamWriter->getPos();
	pdfWriter->writeTrailer(trailerDict, lastXRefPos, *streamWriter);

	
	// if new revision should be created, moves storePos behind PDF end of file
	// marker and forces CXref reopen to handle new revision - all
	// changed objects are stored in file now.
	if(newRevision)
	{
		kernelPrintDbg(DBG_INFO, "Saving changes as new revision.");
		storePos=streamWriter->getPos();
		kernelPrintDbg(DBG_DBG, "New storePos="<<storePos);

		// forces reinitialization of XRef and CXref internal structures from
		// last xref position
		CXref::reopen(xrefPos);

		// new revision number is added - we insert the newest revision so
		// xrefPos value is stored
		revisions.insert(revisions.begin(), xrefPos);
	}

	kernelPrintDbg(DBG_DBG, "finished");
}

void XRefWriter::collectRevisions()
{
	kernelPrintDbg(DBG_DBG, "");

	// clears revisions if non empty
	if(revisions.size())
	{
		kernelPrintDbg(DBG_DBG, "Clearing revisions container.");
		revisions.clear();
	}

	// starts with newest revision
	size_t off=XRef::lastXRefPos;
	// uses deep copy to prevent problems with original data
	Object * trailer=XRef::trailerDict.clone();
	bool cont=true;

	do
	{
		kernelPrintDbg(DBG_DBG, "XRef offset for "<<revisions.size()<<" revision is"<<off);
		// pushes current offset as last revision
		revisions.push_back(off);

		// gets prev field from current trailer and if it is null object (not
		// present) or doesn't have integer value, jumps out of loop
		Object prev;
		trailer->getDict()->lookupNF("Prev", &prev);
		if(prev.getType()==objNull)
		{
			// objNull doesn't need free
			kernelPrintDbg(DBG_DBG, "No previous revision.");
			break;
		}
		if(prev.getType()!=objInt)
		{
			kernelPrintDbg(DBG_DBG, "Prev doesn't have int value. type="<<prev.getType()<<". Assuming no more revisions.");
			prev.free();
			break;
		}

		// sets new value for off
		off=prev.getInt();
		prev.free();

		// searches for TRAILER_KEYWORD to be able to parse older trailer (one
		// for xref on off position) - this works only for oldstyle XRef tables
		// not XRef streams
		str->setPos(off);
		char buffer[1024];
		memset(buffer, '\0', sizeof(buffer));
		char * ret; 
		while((ret=str->getLine(buffer, sizeof(buffer)-1)))
		{
			if(strstr(buffer, STARTXREF_KEYWORD))
			{
				// we have reached startxref keyword and haven't found trailer
				kernelPrintDbg(DBG_WARN, STARTXREF_KEYWORD<<" found but no trailer.");
				cont=false;
				break;
			}
			
			if(strstr(buffer, TRAILER_KEYWORD))
			{
				// trailer found, parse it and set trailer to parsed one
				Object obj;
				::Parser * parser = new Parser(NULL,
					new Lexer(NULL,
						str->makeSubStream(str->getPos(), gFalse, 0, &obj)));
				// deallocates current trailer and parses one for previous
				// revision
				trailer->free();
				parser->getObj(trailer);
				
				delete parser;
				break;
			}
		}
		if(!ret)
		{
			// stream returned NULL, which means that no more data in stream is
			// available
			kernelPrintDbg(DBG_DBG, "end of stream but no trailer found");
			cont=false;
		}
		
	// continues only if no problem with trailer occures
	}while(cont); 

	// deallocates the oldest one - no problem if the oldest is the newest at
	// the same time, because we have used clone of trailer instance from XRef
	trailer->free();
	gfree(trailer);

	kernelPrintDbg(DBG_INFO, "This document contains "<<revisions.size()<<" revisions.");
}

void XRefWriter::changeRevision(unsigned revNumber)
{
	kernelPrintDbg(DBG_DBG, "revNumber="<<revNumber);
	
	// change to same revision
	if(revNumber==revision)
	{
		// nothing to do, we are already here
		kernelPrintDbg(DBG_INFO, "Revision changed to "<<revNumber);
		return;
	}
	
	// constrains check
	if(revNumber>revisions.size()-1)
	{
		kernelPrintDbg(DBG_ERR, "unkown revision with number="<<revNumber);
		// TODO throw an exception
		return;
	}
	
	// forces CXRef to reopen from revisions[revNumber] offset
	// which points to start of xref section for that revision
	size_t off=revisions[revNumber];
	reopen(off);

	// everything ok, so current revision can be set
	revision=revNumber;
	kernelPrintDbg(DBG_INFO, "Revision changed to "<<revision);
}

size_t XRefWriter::getRevisionEnd()const
{
	// the newest revision ends at eofPos
	if(!revision)
		return eofPos;
	
	StreamWriter * streamWriter=dynamic_cast<StreamWriter *>(str);

	// starts from last xref position
	streamWriter->setPos(lastXRefPos);
	char buffer[BUFSIZ];
	size_t length=0;
	memset(buffer, '\0', sizeof(buffer));
	while(streamWriter->getLine(buffer, sizeof(buffer)))
	{
		if(!strncmp(buffer, STARTXREF_KEYWORD, strlen(STARTXREF_KEYWORD)))
		{
			// we have found startstreamWriter key word, next line should contain
			// value of offset - this information is not important, we just have
			// to get behind and calculates number of bytes
			streamWriter->getLine(buffer, sizeof(buffer));
			break;
		}
	}

	// returns current position
	return streamWriter->getPos();
}

void XRefWriter::cloneRevision(FILE * file)const
{
using namespace debug;

	kernelPrintDbg(DBG_ERR, "");

	StreamWriter * streamWriter=dynamic_cast<StreamWriter *>(str);
	size_t revisionEOF=getRevisionEnd();

	kernelPrintDbg(DBG_DBG, "Copies until "<<revisionEOF<<" offset");
	streamWriter->clone(file, 0, revisionEOF);
}
