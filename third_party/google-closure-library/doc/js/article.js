// Documentation licensed under CC BY 4.0
// License available at https://creativecommons.org/licenses/by/4.0/

// TODO(sdh): Bring in Closure library and compiler.

var closure = window.closure || {};
closure.docs = closure.docs || {};


/** @const {string} */
closure.docs.LOCATION = String(window.location);


/**
 * Returns a value from the global `_JEKYLL_DATA` object, which contains the
 * 'page' and 'site' data from Jekyll.  This allows to configure the JS
 * behavior from the frontmatter.  It is effectively `goog.getObjectByName`,
 * except we're not currently depending on Closure.
 * @param {string} param
 * @return {*} Result, or undefined.
 */
closure.docs.get = function(param) {
  var data = window['_JEKYLL_DATA'];
  return data && data[param];
};


/**
 * Applies the given function to each element.
 * @param {string} selector A query selector.
 * @param {function(!Element)} func
 */
closure.docs.forEachElement = function(selector, func) {
  var elements = document.querySelectorAll(selector);
  for (var i = 0, length = elements.length; i < length; i++) {
    func(elements[i]);
  }
};


/**
 * Runs a callback on each heading.
 * @param {function(!Element, number)} func
 */
closure.docs.forEachHeading = function(func) {
  closure.docs.forEachElement(
      'article > h1, article > h2, article > h3, ' +
          'article > h4, article > h5, article > h6',
      function(heading) {
        var match = /^h(\d)$/i.exec(heading.tagName);
        func(heading, Number(match[1]));
      });
};


/**
 * Adds a scroll listener to the document.
 * The listener adds a "scrolled" and a "down" class to the body element
 * to indicate (respectively) whether the page is scrolled at all, and
 * whether the last scroll was down.
 */
closure.docs.addScrollListener = function() {
  // Add a scroll listener to handle body.scrolled and body.down
  var last = void 0;
  // Height difference between the scrolled and unscrolled header bars
  var threshold = 140;
  document.addEventListener('scroll', function() {
    var top = document.body.scrollTop;
    document.body.classList.toggle('scrolled', top > threshold);
    document.body.classList.toggle('down', top > last);
    last = top;
  });
};


/**
 * Add a listener so that clicking on #-only links calls scrollToHash
 * instead of the browser default.
 */
closure.docs.interceptLinkClicks = function() {
  /**
   * Scrolls to the window's current hash.  Note: this is required
   * because of the {position: fixed} banner at the top, which will
   * cover the heading if we let the browser scroll on its own.
   * Instead, we add a delta to ensure that the heading ends up
   * below the banner.
   */
  function scrollToHash() {
    var hash = window.location.hash.substring(1);
    if (hash) {
      var el = document.getElementById(hash);
      var delta = document.body.classList.contains('scrolled') ? 72 : 128;
      document.body.scrollTop = el.offsetTop - delta;
    }
  }
  document.addEventListener('click', function(e) {
    if (!e.target || e.target.tagName != 'A') return;
    var href = e.target.getAttribute('href');
    if (href && href[0] == '#') {
      window.location.hash = href;
      requestAnimationFrame(scrollToHash);
      e.preventDefault();
    }
  });
  // Also scroll to hash on initial page load.
  requestAnimationFrame(scrollToHash);
};


/**
 * Removes the first <h1> header in the article and writes it into
 * the header and title.  This should be done before building the
 * TOC so that the title doesn't show up as an entry.
 */
closure.docs.findTitle = function() {
  // Note: we need to skip the first (#top_of_page) element.
  var h1 = document.querySelectorAll('article > h1')[1];
  if (h1) {
    var pageTitle = h1.textContent;
    h1.remove();
    var title = document.querySelector('title');
    if (!title.textContent) title.textContent = pageTitle;
    var heading = document.querySelector('h1#top_of_page');
    if (heading && !heading.textContent) heading.textContent = pageTitle;
  }
};


/**
 * Iterates over heading elements to add/correct numbers.
 * Anything that looks like a number will be adjusted.
 * Specifically, one can simply write "### 1.1" for all
 * headings and this function will fill in the correct
 * number.  Also assigns IDs if one isn't already given.
 */
closure.docs.autoNumber = function() {
  var min = Number(closure.docs.get('page.toc.min') || 2);
  var nums = [];
  var ids = {};
  closure.docs.forEachHeading(function(heading, level) {
    if (level < min) return;
    // Don't do any numbering unless the heading starts with a digit,
    // though we do still need to pop numbers off before incrementing.
    while (nums.length > level - min + 1) {
      nums.pop();
    }
    if (!/^\d/.test(heading.textContent)) return;
    while (nums.length < level - min + 1) {
      nums.push(0);
    }
    nums[nums.length - 1]++;
    // Auto-generate an ID if necessary.
    if (!heading.id) {
      var base = '_' +
          heading.textContent.toLowerCase()
              .replace(/[^a-z]+/g, '-')
              .replace(/^-|-$/g, '');
      var suffix = '';
      while (base + suffix in ids) {
        suffix++;
      }
      heading.id = base + suffix;
      ids[base + suffix] = true;
    }
    // Correct the number.
    heading.textContent =
        heading.textContent.replace(/^\d+(\.\d+)*/, nums.join('.'));
  });
};


/**
 * Replaces the text content of intra-document links to match the
 * linked section's heading.  This is necessary when auto-numbering
 * is used in order to get the right number in the text.  It is
 * triggered by links whose text is exactly two or more question marks.
 */
closure.docs.fixLinkText = function() {
  closure.docs.forEachElement('a', function(link) {
    var href = link.getAttribute('href');
    if (!/^#/.test(href) || !/^\?\?+$/.test(link.textContent)) return;
    var heading = document.getElementById(href.substring(1));
    if (heading) link.textContent = heading.textContent;
    // TODO(sdh): allow including/excluding the number?
  });
};


/**
 * Builds the table of contents.  This should run after
 * autoNumber so that the correct text makes it in.
 */
closure.docs.buildToc = function() {
  // Read a few page-level parameters to customize.
  var min = Number(closure.docs.get('page.toc.min') || 2);
  var max = Number(closure.docs.get('page.toc.max') || 3);
  // TODO(sdh): allow further customization of numbering?
  var stack = [];
  closure.docs.forEachHeading(function(heading, level) {
    if (level < min || level > max) return;
    var depth = level - min + 1;
    while (stack.length > depth) {
      stack.pop();
    }
    while (stack.length < depth) {
      var list = document.createElement('ul');
      // Add to the most recent 'li' item (unless this is the first entry).
      var prev = stack[stack.length - 1];
      if (prev) {
        if (!prev.lastChild) prev.appendChild(document.createElement('li'));
        prev.lastChild.appendChild(list);
      }
      stack.push(list);
    }
    var item = document.createElement('li');
    stack[stack.length - 1].appendChild(item);
    var link = document.createElement('a');
    item.appendChild(link);
    link.href = '#' + heading.id;
    link.textContent = heading.textContent;
  });

  // Finally add the toc to our toc elements.
  var toc = stack[0];
  closure.docs.forEachElement('nav.toc ul', function(ul) {
    if (toc && toc.innerHTML) {
      ul.innerHTML += toc.innerHTML;
    } else {
      ul.parentElement.remove();  // don't bother with TOC if it's empty
    }
  });
};


/**
 * Fix some syntax highlighting.  Rouge does a poor job highlighting JS.
 * It marks every identifier as 'nx' regardless of how it is used, whereas
 * GitHub-flavored markdown highlights the final identifier in a qualified
 * function name as a function.  This function finds any 'nx' identifier
 * that is followed by an open-paren and changes it to 'nf'.
 */
closure.docs.fixSyntaxHighlighting = function() {
  closure.docs.forEachElement('.highlight .nx+.p', function(p) {
    if (p.textContent[0] == '(') p.previousElementSibling.className = 'nf';
  });
};


/**
 * Highlights callouts.  A callout is a paragraph that begins with
 * 'NOTE:' or 'TIP:' or 'WARNING:' (or several others).  These are
 * highlighted by adding the 'callout-*' to the classlist, where
 * '*' is 'note', 'tip', 'warning', etc.
 */
closure.docs.highlightCallouts = function() {
  closure.docs.forEachElement('p', function(p) {
    var match = /^([A-Za-z]+):/.exec(p.textContent);
    if (match) p.classList.add('callout-' + match[1].toLowerCase());
  });
};


/**
 * Sets the URL on the edit link.
 */
closure.docs.setEditLink = function() {
  var link = document.querySelector('a.edit');
  var match =
      /\/\/([^.]+).github.io\/([^/]+)\/(.*)$/.exec(closure.docs.LOCATION);
  if (!match || !link) return;
  link.href = [
    'https://github.com', match[1], match[2], 'edit/master/doc',
    match[3] + '.md'
  ].join('/');
};


/**
 * Marks the current page and section as 'active' in nav menus.
 */
closure.docs.markActiveNav = function() {
  // Absolutize link
  var abs = (function() {
    var link = document.createElement('a');
    return function(rel) {
      link.href = rel;
      return link.href;
    };
  })();

  // Checks for a prefix, returns everything after it if it exists
  var suffix = function(prefix, string) {
    return string.substring(0, prefix.length) == prefix ?
        string.substring(prefix.length) :
        '';
  };

  // Figure out the current page/section.
  var location = closure.docs.LOCATION;
  var page = location.replace(/\.(?:md|html)?/, '');
  var section = location.substring(0, location.lastIndexOf('/'));

  // If section was overridden in the page frontmatter, use that instead.
  var sectionParameter = closure.docs.get('page.section');
  if (sectionParameter != null) {
    var root = closure.docs.get('site.baseurl;') || '/';
    if (root.length > 1 && root[root.length - 1] == '/') {
      root = root.substring(0, root.length - 1);
    }
    section = abs(root + '/' + sectionParameter.replace(/^\/|\/$/g, ''));
  }

  // Set links active if we're currently visiting them.
  closure.docs.forEachElement('header nav a', function(a) {
    if (/^\/[^/]*$/.test(suffix(section, a.href))) {
      a.classList.add('active');
    }
  });
  closure.docs.forEachElement('nav.side a', function(a) {
    if (/^(\.html|\.md)?$/.test(suffix(page, a.href))) {
      a.classList.add('active');
    }
  });
};


/**
 * Kicks off Google Analytics.  This is just a pretty-printed
 * version of the standard installation code.
 * @suppress {checkTypes}
 */
closure.docs.startAnalytics = function() {
  var productKey = closure.docs.get('page.ga');
  if (!productKey) return;
  (function(i, s, o, g, r, a, m) {
    i['GoogleAnalyticsObject'] = r;
    i[r] = i[r] || function() {
      (i[r].q = i[r].q || []).push(arguments);
    }, i[r].l = 1 * new Date();
    a = s.createElement(o), m = s.getElementsByTagName(o)[0];
    a.async = 1;
    a.src = g;
    m.parentNode.insertBefore(a, m);
  })(window, document, 'script', '//www.google-analytics.com/analytics.js',
     'ga');
  window['ga']('create', productKey, 'auto');
  window['ga']('send', 'pageview');
};


/**
 * Initialize everything.
 */
closure.docs.initialize = function() {
  closure.docs.findTitle();
  closure.docs.autoNumber();
  closure.docs.buildToc();
  closure.docs.fixLinkText();
  closure.docs.fixSyntaxHighlighting();
  closure.docs.highlightCallouts();
  closure.docs.markActiveNav();
  closure.docs.addScrollListener();
  closure.docs.interceptLinkClicks();
  closure.docs.setEditLink();
  closure.docs.startAnalytics();
};
